#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

"""Program paths come from CMake's file API reply, not from the manifest."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from file_api_fixture import write_reply  # noqa: E402
from src.configs.cmake_file_api import (
    CMakeReplyError,
    has_reply,
    multi_config_name,
    query_path,
    target_artifact,
    write_query,
)
from src.configs.justfile import generate_justfile_run_configs
from src.configs.run_configs import resolve_target_programs
from src.core.ui import TextUI


class TestQuery(unittest.TestCase):
    def test_query_is_an_empty_file_named_for_the_client(self):
        with tempfile.TemporaryDirectory() as d:
            write_query(d)
            path = os.path.join(d, ".cmake/api/v1/query/client-mrdocs/codemodel-v2")
            self.assertEqual(query_path(d), path)
            self.assertTrue(os.path.isfile(path))
            self.assertEqual(os.path.getsize(path), 0)

    def test_dry_run_writes_nothing(self):
        with tempfile.TemporaryDirectory() as d:
            write_query(d, dry_run=True, ui=TextUI())
            self.assertFalse(os.path.exists(query_path(d)))


class TestTargetArtifact(unittest.TestCase):
    def test_executable_path_relative_to_the_build_dir(self):
        with tempfile.TemporaryDirectory() as d:
            write_reply(d)
            self.assertEqual(target_artifact(d, "mrdocs"), "tools/mrdocs/mrdocs")
            self.assertEqual(
                target_artifact(d, "mrdocs-unit-tests"), "tests/unit/mrdocs-unit-tests")

    def test_newest_reply_wins(self):
        with tempfile.TemporaryDirectory() as d:
            write_reply(d, {"mrdocs": "old/mrdocs"}, stamp="2026-01-01T000000-0000")
            write_reply(d, {"mrdocs": "new/mrdocs"}, stamp="2026-02-01T000000-0000")
            self.assertEqual(target_artifact(d, "mrdocs"), "new/mrdocs")

    def test_multi_config_picks_the_named_configuration_else_the_first(self):
        with tempfile.TemporaryDirectory() as d:
            write_reply(d, {"mrdocs": "tools/mrdocs/mrdocs.exe"},
                        configurations=("Debug", "Release"))
            self.assertEqual(
                target_artifact(d, "mrdocs", "release"), "tools/mrdocs/Release/mrdocs.exe")
            self.assertEqual(
                target_artifact(d, "mrdocs", "RelWithDebInfo"), "tools/mrdocs/Debug/mrdocs.exe")
            self.assertEqual(target_artifact(d, "mrdocs"), "tools/mrdocs/Debug/mrdocs.exe")

    def test_no_reply_is_an_error_that_names_the_query(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertFalse(has_reply(d))
            with self.assertRaisesRegex(CMakeReplyError, "no CMake file API reply"):
                target_artifact(d, "mrdocs")

    def test_reply_without_a_code_model_is_an_error(self):
        with tempfile.TemporaryDirectory() as d:
            reply = os.path.join(d, ".cmake/api/v1/reply")
            os.makedirs(reply)
            with open(os.path.join(reply, "index-1.json"), "w") as f:
                json.dump({"objects": [], "reply": {}}, f)
            with self.assertRaisesRegex(CMakeReplyError, "no code model"):
                target_artifact(d, "mrdocs")

    def test_unknown_target_is_an_error(self):
        with tempfile.TemporaryDirectory() as d:
            write_reply(d)
            with self.assertRaisesRegex(CMakeReplyError, "not in the CMake file API reply"):
                target_artifact(d, "nonesuch")

    def test_library_target_is_not_a_program(self):
        with tempfile.TemporaryDirectory() as d:
            write_reply(d)
            reply = os.path.join(d, ".cmake/api/v1/reply")
            path = os.path.join(reply, "target-mrdocs-x-abc.json")
            with open(path, "w") as f:
                json.dump({"name": "mrdocs", "type": "STATIC_LIBRARY", "artifacts": []}, f)
            with self.assertRaisesRegex(CMakeReplyError, "not an EXECUTABLE"):
                target_artifact(d, "mrdocs")


class TestManifest(unittest.TestCase):
    def test_manifest_has_no_hand_written_program_paths(self):
        path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                            "src", "configs", "run_configs.json")
        with open(path) as f:
            configs = json.load(f)["configs"]
        self.assertTrue(any("target" in c for c in configs))
        for config in configs:
            self.assertNotIn("program", config, config["name"])


class TestGeneratorsTakeThePathFromTheReply(unittest.TestCase):
    def test_target_entries_get_the_reported_program(self):
        with tempfile.TemporaryDirectory() as d:
            write_reply(d, {"mrdocs": "bin/odd/mrdocs-renamed"})
            configs = [{"name": "Version", "target": "mrdocs"},
                       {"name": "Script", "script": "x.py"}]
            resolve_target_programs(configs, d)
            self.assertEqual(configs[0]["program"], os.path.join(d, "bin", "odd", "mrdocs-renamed"))
            self.assertNotIn("program", configs[1])

    def test_missing_reply_names_the_entry(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError, "'Unit Tests'.*no CMake file API reply"):
                resolve_target_programs(
                    [{"name": "Unit Tests", "target": "mrdocs-unit-tests"}], d)

    def test_dry_run_tolerates_a_missing_reply(self):
        with tempfile.TemporaryDirectory() as d:
            configs = [{"name": "Unit Tests", "target": "mrdocs-unit-tests"}]
            resolve_target_programs(configs, d, dry_run=True)
            self.assertNotIn("program", configs[0])

    def test_justfile_recipe_names_the_reported_program_of_each_preset(self):
        with tempfile.TemporaryDirectory() as d:
            release = os.path.join(d, "build", "release-macos")
            debug = os.path.join(d, "build", "debug-macos")
            write_reply(release, {"mrdocs": "tools/mrdocs/mrdocs"})
            write_reply(debug, {"mrdocs": "other/place/mrdocs"})
            generate_justfile_run_configs(
                [{"name": "Version", "target": "mrdocs", "args": ["--version"]}],
                source_dir=d, build_dir=release, preset="release-macos",
                ui=TextUI(), all_presets=["release-macos", "debug-macos"],
                default_preset="debug-macos",
                justfile_path=os.path.join(d, "justfile"))
            with open(os.path.join(d, "justfile")) as f:
                text = f.read()
            self.assertIn(os.path.join("{{mrdocs}}", "build", "release-macos", "tools", "mrdocs", "mrdocs"), text)
            self.assertIn(os.path.join("{{mrdocs}}", "build", "debug-macos", "other", "place", "mrdocs"), text)

    def test_justfile_without_a_reply_raises(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError, "'Version'"):
                generate_justfile_run_configs(
                    [{"name": "Version", "target": "mrdocs"}],
                    source_dir=d, build_dir=os.path.join(d, "build", "release-macos"),
                    preset="release-macos", ui=TextUI(),
                    all_presets=["release-macos"], default_preset="release-macos",
                    justfile_path=os.path.join(d, "justfile"))

    def test_multi_config_justfile_uses_the_build_type_and_passes_config(self):
        with tempfile.TemporaryDirectory() as d:
            release = os.path.join(d, "build", "release-windows")
            write_reply(release, {"mrdocs": "tools/mrdocs/mrdocs.exe"},
                        configurations=("Debug", "Release"))
            generate_justfile_run_configs(
                [{"name": "Version", "target": "mrdocs"}],
                source_dir=d, build_dir=release, preset="release-windows",
                ui=TextUI(), all_presets=["release-windows"],
                default_preset="release-windows", build_type="Release",
                justfile_path=os.path.join(d, "justfile"))
            with open(os.path.join(d, "justfile")) as f:
                text = f.read()
            self.assertEqual(multi_config_name(release, "release"), "Release")
            self.assertIn("Release", text.split("version-release-windows:")[1].split("\n\n")[0])
            self.assertNotIn(os.path.join("tools", "mrdocs", "Debug"), text)
            self.assertIn("--config Release", text)

    def test_single_config_build_recipes_have_no_config_flag(self):
        with tempfile.TemporaryDirectory() as d:
            release = os.path.join(d, "build", "release-macos")
            write_reply(release)
            self.assertIsNone(multi_config_name(release))
            generate_justfile_run_configs(
                [{"name": "Version", "target": "mrdocs"}],
                source_dir=d, build_dir=release, preset="release-macos",
                ui=TextUI(), all_presets=["release-macos"],
                default_preset="release-macos",
                justfile_path=os.path.join(d, "justfile"))
            with open(os.path.join(d, "justfile")) as f:
                self.assertNotIn("--config", f.read())

    def test_default_preset_missing_a_target_falls_back_to_the_active_one(self):
        with tempfile.TemporaryDirectory() as d:
            release = os.path.join(d, "build", "release-macos")
            debug = os.path.join(d, "build", "debug-macos")
            write_reply(release)
            write_reply(debug, {"mrdocs": "tools/mrdocs/mrdocs"})
            generate_justfile_run_configs(
                [{"name": "Unit Tests", "target": "mrdocs-unit-tests"}],
                source_dir=d, build_dir=release, preset="release-macos",
                ui=TextUI(), all_presets=["release-macos", "debug-macos"],
                default_preset="debug-macos",
                justfile_path=os.path.join(d, "justfile"))
            with open(os.path.join(d, "justfile")) as f:
                self.assertNotIn("unit-tests-debug-macos", f.read())

    def test_unconfigured_default_preset_falls_back_to_the_active_one(self):
        with tempfile.TemporaryDirectory() as d:
            release = os.path.join(d, "build", "release-macos")
            write_reply(release)
            generate_justfile_run_configs(
                [{"name": "Version", "target": "mrdocs"}],
                source_dir=d, build_dir=release, preset="release-macos",
                ui=TextUI(), all_presets=["release-macos", "debug-macos"],
                default_preset="debug-macos",
                justfile_path=os.path.join(d, "justfile"))
            with open(os.path.join(d, "justfile")) as f:
                text = f.read()
            self.assertNotIn("version-debug-macos", text)


if __name__ == "__main__":
    unittest.main()
