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

"""The refresh-only path keeps the saved arguments of the bootstrap entries."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

from src.configs.run_configs import (
    generate_run_configs,
    get_dynamic_run_configs,
    load_saved_bootstrap_args,
)
from src.core.options import InstallOptions
from src.core.ui import TextUI
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from file_api_fixture import write_reply  # noqa: E402


def _options(source_dir):
    return InstallOptions(
        source_dir=source_dir,
        preset="debug",
        build_dir=os.path.join(source_dir, "build", "debug"),
        install_dir=os.path.join(source_dir, "install", "debug"),
        build_type="Debug",
    )


def _write_launch(source_dir, update_args, refresh_args):
    vscode = os.path.join(source_dir, ".vscode")
    os.makedirs(vscode)
    entry = lambda name, args: {"name": name, "type": None, "request": "launch", "args": args}
    with open(os.path.join(vscode, "launch.json"), "w") as f:
        json.dump({"version": "0.2.0", "configurations": [
            entry("Bootstrap Update (debug)", update_args),
            entry("Bootstrap Refresh (debug)", refresh_args),
        ]}, f)


def _saved_entries(source_dir):
    with open(os.path.join(source_dir, ".vscode", "launch.json")) as f:
        data = json.load(f)
    return {c["name"]: c["args"] for c in data["configurations"]}


class TestRefreshKeepsSavedArgs(unittest.TestCase):
    def test_refresh_preserves_extra_flags(self):
        with tempfile.TemporaryDirectory() as src:
            full = ["--preset", "debug", "--build-type", "Debug",
                    "--cc", "/opt/gcc/bin/gcc", "--cxx", "/opt/gcc/bin/g++",
                    "--build-dir", "${workspaceFolder}/build/debug",
                    "--install-dir", "${workspaceFolder}/install/debug"]
            _write_launch(src, full, full + ["--non-interactive"])
            opts = _options(src)
            write_reply(opts.build_dir)
            generate_run_configs(
                opts, InstallOptions(), generate_clion=False, generate_vscode=True,
                generate_vs=False, generate_justfile=False, ui=TextUI(),
                saved_args=load_saved_bootstrap_args(src))
            entries = _saved_entries(src)
            for name in ("Bootstrap Update (debug)", "Bootstrap Refresh (debug)"):
                args = entries[name]
                self.assertIn("--cc", args, name)
                self.assertEqual(args[args.index("--cc") + 1], "/opt/gcc/bin/gcc")
                self.assertEqual(args[args.index("--cxx") + 1], "/opt/gcc/bin/g++")
                self.assertEqual(args[args.index("--build-type") + 1], "Debug")
            self.assertIn("--non-interactive", entries["Bootstrap Refresh (debug)"])
            self.assertNotIn("--non-interactive", entries["Bootstrap Update (debug)"])

    def test_missing_path_flags_are_added(self):
        with tempfile.TemporaryDirectory() as src:
            _write_launch(src, ["--cc", "/usr/bin/clang"], ["--cc", "/usr/bin/clang"])
            saved = load_saved_bootstrap_args(src)
            configs = get_dynamic_run_configs(_options(src), InstallOptions(), saved_args=saved)
            update = next(c for c in configs if c["name"] == "Bootstrap Update (debug)")
            self.assertEqual(update["args"][:2], ["--cc", "/usr/bin/clang"])
            for flag in ("--preset", "--build-dir", "--install-dir"):
                self.assertEqual(update["args"].count(flag), 1)

    def test_no_saved_entry_writes_reduced_set(self):
        with tempfile.TemporaryDirectory() as src:
            self.assertEqual(load_saved_bootstrap_args(src), {})
            configs = get_dynamic_run_configs(_options(src), InstallOptions(), saved_args={})
            refresh = next(c for c in configs if c["name"] == "Bootstrap Refresh (debug)")
            self.assertIn("--non-interactive", refresh["args"])
            self.assertNotIn("--cc", refresh["args"])


if __name__ == "__main__":
    unittest.main()


class RefreshScriptRuns(unittest.TestCase):
    """The script itself parses its arguments and reaches main()."""

    def test_help_exits_zero(self):
        import subprocess
        script = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "refresh_ide_configs.py")
        result = subprocess.run(
            [sys.executable, script, "--help"],
            capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--preset", result.stdout)
