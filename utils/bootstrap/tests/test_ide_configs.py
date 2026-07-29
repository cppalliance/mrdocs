#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

"""Tests for IDE config generators: clion.py, vscode.py, visual_studio.py."""

import json
import os
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from io import StringIO
from unittest.mock import patch, MagicMock, call

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

from src.configs.clion import generate_clion_run_configs
from src.configs.vscode import (
    generate_vscode_run_configs,
    replace_with_placeholders,
    to_task_args,
)
from src.configs.visual_studio import (
    generate_visual_studio_run_configs,
    vs_config_type,
    rel_to_mrdocs_dir,
)
from src.core.ui import TextUI


def _ui():
    return TextUI()


# ── CLion tests ──────────────────────────────────────────────────────────


class TestGenerateClionCMakeTarget(unittest.TestCase):
    """CLion: cmake target configurations."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_cmake_target_basic(self, mock_ensure, mock_write):
        configs = [{"name": "Run App", "target": "mrdocs", "args": ["--help"]}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        mock_ensure.assert_called_once()
        xml_str = mock_write.call_args[0][1]
        self.assertIn("CMakeRunConfiguration", xml_str)
        self.assertIn("mrdocs", xml_str)

    @patch("src.configs.clion.ET.ElementTree.write")
    @patch("src.configs.clion.ensure_dir")
    def test_cmake_target_xml_structure(self, mock_ensure, mock_tree_write):
        configs = [{"name": "Run App", "target": "mrdocs", "args": ["--verbose"], "folder": "Tools"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        mock_tree_write.assert_called_once()
        # Inspect the ElementTree that was used
        args, kwargs = mock_tree_write.call_args
        self.assertIn("encoding", kwargs)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_cmake_target_with_env(self, mock_ensure, mock_write):
        configs = [{"name": "Env App", "target": "mrdocs", "env": {"FOO": "bar", "BAZ": "qux"}}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        written_xml = mock_write.call_args[0][1]
        self.assertIn("FOO", written_xml)
        self.assertIn("bar", written_xml)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_cmake_target_default_preset(self, mock_ensure, mock_write):
        configs = [{"name": "App", "target": "mrdocs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "", dry_run=True, ui=_ui()
        )
        written_xml = mock_write.call_args[0][1]
        self.assertIn('CONFIG_NAME="debug"', written_xml)


class TestGenerateClionPythonScript(unittest.TestCase):
    """CLion: python script configurations."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_python_script(self, mock_ensure, mock_write):
        configs = [{"name": "Bootstrap", "script": "/src/bootstrap.py", "args": ["--yes"]}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("PythonConfigurationType", xml_str)
        self.assertIn("SCRIPT_NAME", xml_str)
        self.assertIn("PYTHONUNBUFFERED", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_python_script_custom_cwd(self, mock_ensure, mock_write):
        configs = [{"name": "Py", "script": "/src/tool.py", "cwd": "/other/dir"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("/other/dir", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_python_script_default_cwd(self, mock_ensure, mock_write):
        configs = [{"name": "Py", "script": "/src/tool.py", "cwd": "/src"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("$PROJECT_DIR$", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_python_script_with_folder(self, mock_ensure, mock_write):
        configs = [{"name": "Py", "script": "/src/tool.py", "folder": "Scripts"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn('folderName="Scripts"', xml_str)


class TestGenerateClionShellScript(unittest.TestCase):
    """CLion: shell script configurations."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_shell_script(self, mock_ensure, mock_write):
        configs = [{"name": "Build Docs", "script": "/src/docs/build.sh"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("ShConfigurationType", xml_str)
        self.assertIn("SCRIPT_PATH", xml_str)
        self.assertIn("EXECUTE_IN_TERMINAL", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_shell_script_custom_cwd(self, mock_ensure, mock_write):
        configs = [{"name": "Sh", "script": "/src/run.sh", "cwd": "/tmp"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("/tmp", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_shell_script_default_cwd(self, mock_ensure, mock_write):
        configs = [{"name": "Sh", "script": "/src/run.sh"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("$PROJECT_DIR$", xml_str)


class TestGenerateClionJSScript(unittest.TestCase):
    """CLion: JavaScript/Node.js configurations."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_js_script(self, mock_ensure, mock_write):
        configs = [{"name": "Lint", "script": "/src/lint.js", "cwd": "/src"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("NodeJSConfigurationType", xml_str)
        self.assertIn("path-to-js-file", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_js_script_with_env(self, mock_ensure, mock_write):
        configs = [{"name": "JS", "script": "/src/app.js", "env": {"NODE_ENV": "dev"}}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("NODE_ENV", xml_str)
        self.assertIn("dev", xml_str)


class TestGenerateClionNpm(unittest.TestCase):
    """CLion: npm configurations."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_npm_script(self, mock_ensure, mock_write):
        configs = [{"name": "NPM CI", "script": "npm", "cwd": "/src/docs", "args": ["ci"]}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("js.build_tools.npm", xml_str)
        self.assertIn("package.json", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_npm_no_args_defaults_to_ci(self, mock_ensure, mock_write):
        configs = [{"name": "NPM", "script": "npm", "cwd": "/src/docs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        root = ET.fromstring(xml_str.rstrip("\n"))
        command_el = root.find(".//command")
        self.assertIsNotNone(command_el)
        self.assertEqual(command_el.get("value"), "ci")

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_npm_with_folder(self, mock_ensure, mock_write):
        configs = [{"name": "NPM", "script": "npm", "cwd": "/src/docs", "folder": "Docs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn('folderName="Docs"', xml_str)


class TestGenerateClionGenericFallback(unittest.TestCase):
    """CLion: generic shell fallback for unknown script types."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_generic_script(self, mock_ensure, mock_write):
        configs = [{"name": "Trang", "script": "/usr/bin/trang", "args": ["-I", "rnc"]}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("ShConfigurationType", xml_str)
        self.assertIn("SCRIPT_PATH", xml_str)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_generic_no_args(self, mock_ensure, mock_write):
        configs = [{"name": "Tool", "script": "/usr/bin/tool"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        xml_str = mock_write.call_args[0][1]
        self.assertIn("ShConfigurationType", xml_str)


class TestClionRunConfigDir(unittest.TestCase):
    """CLion: run_config_dir and dry_run behavior."""

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_custom_run_config_dir(self, mock_ensure, mock_write):
        configs = [{"name": "App", "target": "mrdocs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release",
            run_config_dir="/custom/.run", dry_run=True, ui=_ui()
        )
        mock_ensure.assert_called_once_with("/custom/.run", dry_run=True, ui=unittest.mock.ANY)
        path_written = mock_write.call_args[0][0]
        self.assertTrue(path_written.startswith("/custom/.run/"))

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_default_run_config_dir(self, mock_ensure, mock_write):
        configs = [{"name": "App", "target": "mrdocs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        mock_ensure.assert_called_once_with("/src/.run", dry_run=True, ui=unittest.mock.ANY)

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_dry_run_calls_write_text(self, mock_ensure, mock_write):
        configs = [{"name": "App", "target": "mrdocs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        mock_write.assert_called_once()
        _, kwargs = mock_write.call_args
        self.assertTrue(kwargs.get("dry_run", False))

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_multiple_configs(self, mock_ensure, mock_write):
        configs = [
            {"name": "App", "target": "mrdocs"},
            {"name": "Py", "script": "/src/tool.py"},
            {"name": "Sh", "script": "/src/run.sh"},
        ]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        self.assertEqual(mock_write.call_count, 3)

    @patch("src.configs.clion.get_default_ui")
    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_default_ui(self, mock_ensure, mock_write, mock_get_ui):
        mock_get_ui.return_value = TextUI()
        configs = [{"name": "App", "target": "mrdocs"}]
        generate_clion_run_configs(configs, "/src", "/src/build", "release", dry_run=True)
        mock_get_ui.assert_called_once()


# ── VSCode tests ─────────────────────────────────────────────────────────


class TestVSCodeHelpers(unittest.TestCase):
    """VSCode: replace_with_placeholders and to_task_args."""

    def test_replace_with_placeholders_strings(self):
        cfg = {"cwd": "/src/build", "program": "/src/bin/app"}
        replace_with_placeholders(cfg, "/src")
        self.assertEqual(cfg["cwd"], "${workspaceFolder}/build")
        self.assertEqual(cfg["program"], "${workspaceFolder}/bin/app")

    def test_replace_with_placeholders_lists(self):
        cfg = {"args": ["/src/file1", "/src/file2", "plain"]}
        replace_with_placeholders(cfg, "/src")
        self.assertEqual(cfg["args"], ["${workspaceFolder}/file1", "${workspaceFolder}/file2", "plain"])

    def test_replace_with_placeholders_nested_dicts(self):
        cfg = {"env": {"PATH": "/src/bin:/usr/bin"}}
        replace_with_placeholders(cfg, "/src")
        self.assertEqual(cfg["env"]["PATH"], "${workspaceFolder}/bin:/usr/bin")

    def test_replace_with_placeholders_no_match(self):
        cfg = {"key": "no match here"}
        replace_with_placeholders(cfg, "/src")
        self.assertEqual(cfg["key"], "no match here")

    def test_to_task_args_with_args(self):
        cfg = {"args": ["--help", "--verbose"]}
        result = to_task_args(cfg)
        self.assertEqual(result, ["--help", "--verbose"])
        # Verify it's a copy
        result.append("extra")
        self.assertEqual(len(cfg["args"]), 2)

    def test_to_task_args_no_args(self):
        self.assertEqual(to_task_args({}), [])

    def test_to_task_args_non_list(self):
        self.assertEqual(to_task_args({"args": "string"}), [])


class TestGenerateVSCodeTarget(unittest.TestCase):
    """VSCode: cmake target launch configurations."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_cmake_target_creates_launch_and_tasks(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "mrdocs", "target": "mrdocs", "args": ["--help"]}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        self.assertEqual(mock_write.call_count, 2)  # launch.json + tasks.json
        launch_call = mock_write.call_args_list[0]
        launch_data = json.loads(launch_call[0][1])
        self.assertEqual(launch_data["version"], "0.2.0")
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["type"], "cppdbg")
        self.assertIn("preLaunchTask", cfg)

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_cmake_target_build_task_created(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "mrdocs", "target": "mrdocs"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_call = mock_write.call_args_list[1]
        tasks_data = json.loads(tasks_call[0][1])
        task_labels = [t["label"] for t in tasks_data["tasks"]]
        self.assertIn("CMake Configure (release)", task_labels)
        self.assertIn("CMake Build mrdocs (release)", task_labels)

    @patch("src.configs.vscode.os.path.exists", return_value=False)
    @patch("src.configs.vscode.shutil.which", return_value=None)
    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_cmake_target_no_debugger(self, mock_ensure, mock_load, mock_write, mock_which, mock_exists):
        configs = [{"name": "app", "target": "app"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release",
            compiler_info={"CMAKE_CXX_COMPILER_ID": "GNU"},
            dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertNotIn("MIMode", cfg)

    @patch("src.configs.vscode.shutil.which", return_value="/usr/bin/gdb")
    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_cmake_target_gdb_detected_for_gcc(self, mock_ensure, mock_load, mock_write, mock_which):
        """GCC compiler should use GDB debugger, not LLDB."""
        configs = [{"name": "app", "target": "app"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release",
            compiler_info={"CMAKE_CXX_COMPILER_ID": "GNU"},
            dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg.get("MIMode"), "gdb")
        self.assertEqual(cfg.get("miDebuggerPath"), "/usr/bin/gdb")

    @patch("src.configs.vscode.shutil.which", return_value="/usr/bin/lldb")
    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_cmake_target_lldb_detected_for_clang(self, mock_ensure, mock_load, mock_write, mock_which):
        """Clang compiler should use LLDB debugger."""
        configs = [{"name": "app", "target": "app"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release",
            compiler_info={"CMAKE_CXX_COMPILER_ID": "Clang"},
            dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg.get("MIMode"), "lldb")
        self.assertEqual(cfg.get("miDebuggerPath"), "/usr/bin/lldb")

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_cmake_target_with_program(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "app", "target": "app", "program": "/custom/bin/app"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["program"], "/custom/bin/app")


class TestGenerateVSCodePythonScript(unittest.TestCase):
    """VSCode: python script launch configurations."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_python_script(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Bootstrap", "script": "/src/bootstrap.py", "args": ["--yes"]}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["type"], "debugpy")
        self.assertEqual(cfg["console"], "integratedTerminal")
        self.assertTrue("justMyCode" in cfg)


class TestGenerateVSCodeJSScript(unittest.TestCase):
    """VSCode: JavaScript script launch configurations."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_js_script(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Lint", "script": "/src/lint.js", "env": {"NODE_ENV": "test"}}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["type"], "node")
        self.assertIn("skipFiles", cfg)
        self.assertEqual(cfg["env"]["NODE_ENV"], "test")


class TestGenerateVSCodeShellTask(unittest.TestCase):
    """VSCode: shell script task configurations."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_shell_script_becomes_task(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Build Docs", "script": "/src/docs/build.sh", "args": ["--clean"], "cwd": "/other"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task_labels = [t["label"] for t in tasks_data["tasks"]]
        self.assertIn("Build Docs", task_labels)
        build_docs_task = next(t for t in tasks_data["tasks"] if t["label"] == "Build Docs")
        self.assertEqual(build_docs_task["type"], "shell")

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_npm_script_becomes_task(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "NPM CI", "script": "npm", "args": ["ci"], "cwd": "/src/docs"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        # npm is not .py/.js and has no target, so it's a task
        task_labels = [t["label"] for t in tasks_data["tasks"]]
        self.assertIn("NPM CI", task_labels)


class TestGenerateVSCodePreset(unittest.TestCase):
    """VSCode: preset and cmake config behavior."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_preset_in_cmake_configure(self, mock_ensure, mock_load, mock_write):
        configs = []
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        config_task = tasks_data["tasks"][0]
        self.assertIn("--preset", config_task["args"])
        self.assertIn("release", config_task["args"])

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_no_preset_uses_build_dir(self, mock_ensure, mock_load, mock_write):
        configs = []
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        config_task = tasks_data["tasks"][0]
        self.assertIn("-B", config_task["args"])

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_no_preset_with_ninja(self, mock_ensure, mock_load, mock_write):
        configs = []
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "", ninja_path="/usr/bin/ninja",
            dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        config_task = tasks_data["tasks"][0]
        self.assertIn("-G", config_task["args"])
        self.assertIn("Ninja", config_task["args"])

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_unknown_script_becomes_task(self, mock_ensure, mock_load, mock_write):
        """Scripts that are not .py/.js and have no target become tasks."""
        configs = [{"name": "Tool", "script": "/src/tool.rb"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task_labels = [t["label"] for t in tasks_data["tasks"]]
        self.assertIn("Tool", task_labels)


class TestGenerateVSCodeExistingConfigs(unittest.TestCase):
    """VSCode: merging with existing launch.json/tasks.json."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file")
    @patch("src.configs.vscode.ensure_dir")
    def test_merges_existing_launch(self, mock_ensure, mock_load, mock_write):
        existing_launch = {
            "version": "0.2.0",
            "configurations": [{"name": "Existing", "type": "cppdbg", "request": "launch"}]
        }
        existing_tasks = {"version": "2.0.0", "tasks": []}
        mock_load.side_effect = [existing_launch, existing_tasks]
        configs = [{"name": "New", "script": "/src/tool.py"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        names = [c["name"] for c in launch_data["configurations"]]
        self.assertIn("Existing", names)
        self.assertIn("New", names)

    @patch("src.configs.vscode.get_default_ui")
    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_default_ui(self, mock_ensure, mock_load, mock_write, mock_get_ui):
        mock_get_ui.return_value = TextUI()
        configs = []
        generate_vscode_run_configs(configs, "/src", "/src/build", "release", dry_run=False)
        mock_get_ui.assert_called_once()


class TestGenerateVSCodeUniqueTargets(unittest.TestCase):
    """VSCode: unique build tasks for cmake targets."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_duplicate_targets_single_task(self, mock_ensure, mock_load, mock_write):
        configs = [
            {"name": "Test 1", "target": "mrdocs-test"},
            {"name": "Test 2", "target": "mrdocs-test"},
        ]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        build_tasks = [t for t in tasks_data["tasks"] if t["label"].startswith("CMake Build")]
        self.assertEqual(len(build_tasks), 1)


# ── Visual Studio tests ─────────────────────────────────────────────────


class TestVSConfigType(unittest.TestCase):
    """Visual Studio: vs_config_type helper."""

    def test_python_script(self):
        self.assertEqual(vs_config_type({"script": "foo.py"}), "python")

    def test_js_script(self):
        self.assertEqual(vs_config_type({"script": "foo.js"}), "nodejs")

    def test_shell_script(self):
        self.assertEqual(vs_config_type({"script": "foo.sh"}), "shell")

    def test_generic_script(self):
        self.assertEqual(vs_config_type({"script": "tool"}), "shell")

    def test_target(self):
        self.assertEqual(vs_config_type({"target": "mrdocs"}), "default")

    def test_no_script_no_target(self):
        self.assertIsNone(vs_config_type({"name": "something"}))


class TestRelToMrdocsDir(unittest.TestCase):
    """Visual Studio: rel_to_mrdocs_dir helper."""

    def test_subdir_converted(self):
        result = rel_to_mrdocs_dir("/src/util/tool.py", "/src")
        self.assertEqual(result, os.path.relpath("/src/util/tool.py", "/src"))

    def test_not_subdir_unchanged(self):
        result = rel_to_mrdocs_dir("/other/tool.py", "/src")
        self.assertEqual(result, "/other/tool.py")

    def test_slash_normalization(self):
        result = rel_to_mrdocs_dir("/src\\util\\tool.py", "/src")
        # On non-Windows, backslashes in paths are literal characters
        # The function normalizes slashes for comparison
        self.assertIsInstance(result, str)

    def test_trailing_slash(self):
        result = rel_to_mrdocs_dir("/src/util/tool.py", "/src/")
        self.assertEqual(result, os.path.relpath("/src/util/tool.py", "/src/"))


class TestGenerateVSTarget(unittest.TestCase):
    """Visual Studio: cmake target configurations."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_cmake_target(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "mrdocs", "target": "mrdocs", "args": ["--help"]}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        self.assertEqual(mock_write.call_count, 2)  # launch.vs.json + tasks.vs.json
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["type"], "default")
        self.assertEqual(cfg["project"], "CMakeLists.txt")
        self.assertEqual(cfg["projectTarget"], "mrdocs.exe")
        self.assertEqual(cfg["args"], ["--help"])

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_cmake_target_with_env_and_cwd(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "app", "target": "app", "cwd": "/work", "env": {"FOO": "bar"}}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["cwd"], "/work")
        self.assertEqual(cfg["env"], {"FOO": "bar"})


class TestGenerateVSPythonScript(unittest.TestCase):
    """Visual Studio: python script configurations."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_python_script(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Bootstrap", "script": "/src/bootstrap.py", "args": ["--yes", "--verbose"]}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        cfg = launch_data["configurations"][0]
        self.assertEqual(cfg["type"], "python")
        self.assertEqual(cfg["interpreter"], "(default)")
        self.assertIn("--yes", cfg["scriptArguments"])
        self.assertFalse(cfg["nativeDebug"])


class TestGenerateVSTaskConfigs(unittest.TestCase):
    """Visual Studio: task configurations for non-launch scripts."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_shell_task(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Build Docs", "script": "/src/docs/build.sh", "args": ["--clean"], "cwd": "/src/docs"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task = tasks_data["tasks"][0]
        self.assertEqual(task["taskLabel"], "Build Docs")
        self.assertEqual(task["type"], "launch")
        self.assertEqual(task["workingDirectory"], "/src/docs")

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_js_task_rewrites_command(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Lint", "script": "/src/lint.js", "args": ["--fix"]}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task = tasks_data["tasks"][0]
        self.assertEqual(task["command"], "node")
        self.assertEqual(task["args"][0], "/src/lint.js")

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_npm_task_applies_to_package_json(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "NPM CI", "script": "npm", "args": ["ci"], "cwd": "/src/docs"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task = tasks_data["tasks"][0]
        self.assertIn("package.json", task["appliesTo"])

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_relaxng_schema_task(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Generate RelaxNG Schema", "script": "trang"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task = tasks_data["tasks"][0]
        self.assertEqual(task["appliesTo"], "docs/modules/ROOT/attachments/schemas/mrdocs.rnc")

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_xml_lint_task(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "XML Lint with RelaxNG Schema", "script": "xmllint"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task = tasks_data["tasks"][0]
        self.assertEqual(task["appliesTo"], "mrdocs.rng")

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_task_with_env(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "Tool", "script": "tool", "env": {"KEY": "val"}}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        task = tasks_data["tasks"][0]
        self.assertEqual(task["env"], {"KEY": "val"})


class TestGenerateVSExistingConfigs(unittest.TestCase):
    """Visual Studio: merging with existing configs."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file")
    @patch("src.configs.visual_studio.ensure_dir")
    def test_merges_existing_launch(self, mock_ensure, mock_load, mock_write):
        existing_launch = {
            "version": "0.2.1",
            "defaults": {},
            "configurations": [{"name": "Old", "type": "default"}]
        }
        existing_tasks = {"version": "0.2.1", "tasks": []}
        mock_load.side_effect = [existing_launch, existing_tasks]
        configs = [{"name": "New", "target": "app"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        names = [c["name"] for c in launch_data["configurations"]]
        self.assertIn("Old", names)
        self.assertIn("New", names)


class TestGenerateVSDryRun(unittest.TestCase):
    """Visual Studio: dry-run and default UI."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_dry_run_passes_flag(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "app", "target": "app"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=True, ui=_ui()
        )
        for c in mock_write.call_args_list:
            self.assertTrue(c[1].get("dry_run", False))

    @patch("src.configs.visual_studio.get_default_ui")
    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_default_ui(self, mock_ensure, mock_load, mock_write, mock_get_ui):
        mock_get_ui.return_value = TextUI()
        configs = []
        generate_visual_studio_run_configs(configs, "/src", "/src/build", "release")
        mock_get_ui.assert_called_once()

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_vs_dir_path(self, mock_ensure, mock_load, mock_write):
        configs = []
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        mock_ensure.assert_called_once_with("/src/.vs", dry_run=False, ui=unittest.mock.ANY)


class TestGenerateVSJSLaunchConfig(unittest.TestCase):
    """Visual Studio: JS script as launch vs task."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_js_script_not_python_goes_to_task(self, mock_ensure, mock_load, mock_write):
        """JS scripts are not is_config (only .py and target are), so they become tasks."""
        configs = [{"name": "JS App", "script": "/src/app.js"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release", dry_run=False, ui=_ui()
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        # JS is NOT in launch configs for VS (only target and .py)
        self.assertEqual(len(launch_data["configurations"]), 0)
        self.assertEqual(len(tasks_data["tasks"]), 1)


# ── Pruning of stale per-preset entries (US-008) ─────────────────────


class TestVSCodePruning(unittest.TestCase):
    """VSCode: prune generated entries for removed presets, keep user ones."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file")
    @patch("src.configs.vscode.ensure_dir")
    def test_prunes_stale_preset_launch(self, mock_ensure, mock_load, mock_write):
        existing_launch = {"version": "0.2.0", "configurations": [
            {"name": "mrdocs (old-preset)", "type": "cppdbg"},
            {"name": "mrdocs (release)", "type": "cppdbg"},
            {"name": "My Custom Thing", "type": "cppdbg"},
        ]}
        existing_tasks = {"version": "2.0.0", "tasks": [
            {"label": "CMake Build mrdocs (old-preset)"},
        ]}
        mock_load.side_effect = [existing_launch, existing_tasks]
        configs = [{"name": "mrdocs", "target": "mrdocs"}]
        generate_vscode_run_configs(
            configs, "/src", "/src/build", "release",
            all_presets=["release"], dry_run=False, ui=_ui(),
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        names = [c["name"] for c in launch_data["configurations"]]
        self.assertNotIn("mrdocs (old-preset)", names)   # pruned
        self.assertIn("mrdocs (release)", names)          # current preset kept
        self.assertIn("My Custom Thing", names)           # user entry kept
        tasks_data = json.loads(mock_write.call_args_list[1][0][1])
        labels = [t["label"] for t in tasks_data["tasks"]]
        self.assertNotIn("CMake Build mrdocs (old-preset)", labels)


class TestVSPruning(unittest.TestCase):
    """Visual Studio: prune generated entries for removed presets."""

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file")
    @patch("src.configs.visual_studio.ensure_dir")
    def test_prunes_stale_preset_launch(self, mock_ensure, mock_load, mock_write):
        existing_launch = {"version": "0.2.1", "defaults": {}, "configurations": [
            {"name": "app (old)"},
            {"name": "Keep Me"},
        ]}
        existing_tasks = {"version": "0.2.1", "tasks": []}
        mock_load.side_effect = [existing_launch, existing_tasks]
        configs = [{"name": "app", "target": "app"}]
        generate_visual_studio_run_configs(
            configs, "/src", "/src/build", "release",
            all_presets=["release"], dry_run=False, ui=_ui(),
        )
        launch_data = json.loads(mock_write.call_args_list[0][0][1])
        names = [c["name"] for c in launch_data["configurations"]]
        self.assertNotIn("app (old)", names)   # pruned (base 'app' is a target)
        self.assertIn("Keep Me", names)        # user entry kept


class TestClionPruning(unittest.TestCase):
    """CLion: delete marked stale files, keep user files and current ones."""

    def test_prunes_marked_stale_files_only(self):
        d = tempfile.mkdtemp()
        # A previously generated (marked) file for a config no longer present.
        with open(os.path.join(d, "Old Thing.run.xml"), "w", encoding="utf-8") as f:
            f.write('<component name="ProjectRunConfigurationManager">'
                    '<!-- mrdocs:generated --></component>')
        # A user-authored file without the marker.
        with open(os.path.join(d, "User Thing.run.xml"), "w", encoding="utf-8") as f:
            f.write('<component name="ProjectRunConfigurationManager"></component>')
        configs = [{"name": "Keep", "target": "mrdocs"}]
        generate_clion_run_configs(
            configs, "/src", "/src/build", "release",
            run_config_dir=d, dry_run=False, ui=_ui(),
        )
        present = set(os.listdir(d))
        self.assertNotIn("Old Thing.run.xml", present)  # marked + stale -> removed
        self.assertIn("User Thing.run.xml", present)    # unmarked -> kept
        self.assertIn("Keep.run.xml", present)          # current -> written

    def test_generated_files_carry_marker(self):
        d = tempfile.mkdtemp()
        generate_clion_run_configs(
            [{"name": "Keep", "target": "mrdocs"}], "/src", "/src/build", "release",
            run_config_dir=d, dry_run=False, ui=_ui(),
        )
        with open(os.path.join(d, "Keep.run.xml"), encoding="utf-8") as f:
            self.assertIn("mrdocs:generated", f.read())


class TestAggregateConfigsSkipped(unittest.TestCase):
    """Interface/aggregate configs (no target/script) are justfile-only and
    must be skipped by the IDE generators (not emitted as empty entries)."""

    @patch("src.configs.vscode.write_text")
    @patch("src.configs.vscode.load_json_file", return_value=None)
    @patch("src.configs.vscode.ensure_dir")
    def test_vscode_skips_aggregate(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "MrDocs Test", "depends": ["MrDocs Unit Tests"]}]
        generate_vscode_run_configs(configs, "/src", "/src/build", "release",
                                    dry_run=False, ui=_ui())
        launch = json.loads(mock_write.call_args_list[0][0][1])
        tasks = json.loads(mock_write.call_args_list[1][0][1])
        names = [c.get("name") for c in launch["configurations"]]
        labels = [t.get("label") for t in tasks["tasks"]]
        self.assertNotIn("MrDocs Test", names)
        self.assertNotIn("MrDocs Test", labels)

    @patch("src.configs.visual_studio.write_text")
    @patch("src.configs.visual_studio.load_json_file", return_value=None)
    @patch("src.configs.visual_studio.ensure_dir")
    def test_vs_skips_aggregate(self, mock_ensure, mock_load, mock_write):
        configs = [{"name": "MrDocs Test", "depends": ["MrDocs Unit Tests"]}]
        generate_visual_studio_run_configs(configs, "/src", "/src/build", "release",
                                           dry_run=False, ui=_ui())
        launch = json.loads(mock_write.call_args_list[0][0][1])
        tasks = json.loads(mock_write.call_args_list[1][0][1])
        self.assertEqual(launch["configurations"], [])
        self.assertEqual(tasks["tasks"], [])

    @patch("src.configs.clion.write_text")
    @patch("src.configs.clion.ensure_dir")
    def test_clion_skips_aggregate(self, mock_ensure, mock_write):
        configs = [{"name": "MrDocs Test", "depends": ["MrDocs Unit Tests"]}]
        generate_clion_run_configs(configs, "/src", "/src/build", "release",
                                   dry_run=True, ui=_ui())
        mock_write.assert_not_called()


if __name__ == "__main__":
    unittest.main()
