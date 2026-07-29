#!/usr/bin/env python3
"""Tests for tools/visual_studio.py — Windows platform mocked so tests run on any OS."""

import json
import os
import subprocess
import unittest
from unittest.mock import patch, MagicMock

# The module caches results with @lru_cache, so we need to clear caches between tests
from src.tools.visual_studio import (
    get_vs_install_locations,
    find_vs_tool,
    probe_msvc_dev_env,
)


class _VSTestBase(unittest.TestCase):
    """Common setUp that clears lru_cache between tests."""

    def setUp(self):
        get_vs_install_locations.cache_clear()
        probe_msvc_dev_env.cache_clear()


# ---------------------------------------------------------------------------
# get_vs_install_locations
# ---------------------------------------------------------------------------
class TestGetVSInstallLocations(_VSTestBase):
    """Tests for get_vs_install_locations."""

    @patch("src.tools.visual_studio.is_windows", return_value=False)
    def test_returns_empty_list_on_non_windows(self, _mock_win):
        self.assertEqual(get_vs_install_locations(), [])

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.is_tool_executable", return_value=False)
    def test_returns_none_when_vswhere_not_found(self, _mock_exec, _mock_win):
        result = get_vs_install_locations()
        self.assertIsNone(result)

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    @patch("src.tools.visual_studio.subprocess.check_output")
    def test_returns_install_paths(self, mock_co, _mock_exec, _mock_win):
        mock_co.return_value = json.dumps([
            {"installationPath": r"C:\VS\2022\Community"},
            {"installationPath": r"C:\VS\2022\Professional"},
        ])
        result = get_vs_install_locations()
        self.assertEqual(result, [r"C:\VS\2022\Community", r"C:\VS\2022\Professional"])

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    @patch("src.tools.visual_studio.subprocess.check_output")
    def test_returns_none_on_empty_json(self, mock_co, _mock_exec, _mock_win):
        mock_co.return_value = "[]"
        self.assertIsNone(get_vs_install_locations())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    @patch("src.tools.visual_studio.subprocess.check_output",
           side_effect=subprocess.CalledProcessError(1, "vswhere"))
    def test_returns_none_on_subprocess_error(self, _mock_co, _mock_exec, _mock_win):
        self.assertIsNone(get_vs_install_locations())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    @patch("src.tools.visual_studio.subprocess.check_output", return_value="not json")
    def test_returns_none_on_invalid_json(self, _mock_co, _mock_exec, _mock_win):
        self.assertIsNone(get_vs_install_locations())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    @patch("src.tools.visual_studio.subprocess.check_output")
    @patch.dict(os.environ, {"ProgramFiles(x86)": r"D:\Program Files (x86)"})
    def test_uses_env_program_files(self, mock_co, mock_exec, _mock_win):
        mock_co.return_value = json.dumps([{"installationPath": r"C:\VS"}])
        get_vs_install_locations()
        # vswhere path should be built from the env var
        call_args = mock_exec.call_args[0][0]
        self.assertIn("D:", call_args)


# ---------------------------------------------------------------------------
# find_vs_tool
# ---------------------------------------------------------------------------
class TestFindVSTool(_VSTestBase):

    @patch("src.tools.visual_studio.is_windows", return_value=False)
    def test_returns_none_on_non_windows(self, _mock_win):
        self.assertIsNone(find_vs_tool("cmake"))

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    def test_returns_none_for_unsupported_tool(self, _mock_win):
        self.assertIsNone(find_vs_tool("rustc"))

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=None)
    def test_returns_none_when_no_vs_installs(self, _mock_locs, _mock_win):
        self.assertIsNone(find_vs_tool("cmake"))

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[])
    def test_returns_none_when_vs_roots_empty(self, _mock_locs, _mock_win):
        self.assertIsNone(find_vs_tool("cmake"))

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2022"])
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    def test_finds_cmake(self, _mock_exec, _mock_locs, _mock_win):
        result = find_vs_tool("cmake")
        self.assertIsNotNone(result)
        self.assertIn("cmake.exe", result)

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2022"])
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    def test_finds_ninja(self, _mock_exec, _mock_locs, _mock_win):
        result = find_vs_tool("ninja")
        self.assertIsNotNone(result)
        self.assertIn("ninja.exe", result)

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2022"])
    @patch("src.tools.visual_studio.is_tool_executable", return_value=True)
    def test_finds_git(self, _mock_exec, _mock_locs, _mock_win):
        result = find_vs_tool("git")
        self.assertIsNotNone(result)
        self.assertIn("git.exe", result)

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2022"])
    @patch("src.tools.visual_studio.is_tool_executable", return_value=False)
    def test_returns_none_when_tool_not_executable(self, _mock_exec, _mock_locs, _mock_win):
        self.assertIsNone(find_vs_tool("cmake"))

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2019", r"C:\VS\2022"])
    @patch("src.tools.visual_studio.is_tool_executable")
    def test_searches_multiple_vs_roots(self, mock_exec, _mock_locs, _mock_win):
        # First VS root has no cmake, second does
        mock_exec.side_effect = lambda p: "2022" in p
        result = find_vs_tool("cmake")
        self.assertIsNotNone(result)
        self.assertIn("2022", result)

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2022"])
    @patch("src.tools.visual_studio.is_tool_executable", return_value=False)
    def test_python_not_in_toolpaths(self, _mock_exec, _mock_locs, _mock_win):
        # python is in vs_tools list but not in toolpaths dict, so path is None
        self.assertIsNone(find_vs_tool("python"))


# ---------------------------------------------------------------------------
# probe_msvc_dev_env
# ---------------------------------------------------------------------------
class TestProbeMsvcDevEnv(_VSTestBase):

    @patch("src.tools.visual_studio.is_windows", return_value=False)
    def test_returns_none_on_non_windows(self, _mock_win):
        self.assertIsNone(probe_msvc_dev_env())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=None)
    def test_returns_none_when_no_vs_roots(self, _mock_locs, _mock_win):
        self.assertIsNone(probe_msvc_dev_env())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=False)
    def test_returns_none_when_vcvarsall_not_found(self, _mock_exists, _mock_locs, _mock_win):
        self.assertIsNone(probe_msvc_dev_env())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_returns_none_on_vcvarsall_failure(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        mock_run.return_value = MagicMock(returncode=1, stderr="error")
        self.assertIsNone(probe_msvc_dev_env())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_parses_post_init_env(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        stdout = (
            "Some preamble line\n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "PATH=C:\\new\\path\n"
            "INCLUDE=C:\\include\n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "trailer\n"
        )
        mock_run.return_value = MagicMock(returncode=0, stdout=stdout)
        with patch.dict(os.environ, {}, clear=True):
            result = probe_msvc_dev_env()
        self.assertIsNotNone(result)
        self.assertEqual(result["PATH"], r"C:\new\path")
        self.assertEqual(result["INCLUDE"], r"C:\include")

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_filters_unchanged_env_vars(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        stdout = (
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "PATH=C:\\new\\path\n"
            "UNCHANGED=same_value\n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
        )
        mock_run.return_value = MagicMock(returncode=0, stdout=stdout)
        with patch.dict(os.environ, {"UNCHANGED": "same_value"}, clear=True):
            result = probe_msvc_dev_env()
        self.assertIn("PATH", result)
        self.assertNotIn("UNCHANGED", result)

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_returns_none_when_no_post_init_header(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        mock_run.return_value = MagicMock(returncode=0, stdout="just some output\n")
        self.assertIsNone(probe_msvc_dev_env())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_returns_none_when_post_init_empty(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        stdout = (
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
        )
        mock_run.return_value = MagicMock(returncode=0, stdout=stdout)
        self.assertIsNone(probe_msvc_dev_env())

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations",
           return_value=[r"C:\VS\2019", r"C:\VS\2022"])
    @patch("src.tools.visual_studio.subprocess.run")
    def test_searches_multiple_roots_for_vcvarsall(self, mock_run, _mock_locs, _mock_win):
        # First root doesn't have vcvarsall, second does
        def fake_exists(path):
            return "2022" in path
        stdout = (
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "LIB=C:\\lib\n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
        )
        mock_run.return_value = MagicMock(returncode=0, stdout=stdout)
        with patch("src.tools.visual_studio.os.path.exists", side_effect=fake_exists):
            with patch.dict(os.environ, {}, clear=True):
                result = probe_msvc_dev_env()
        self.assertIsNotNone(result)
        self.assertEqual(result["LIB"], r"C:\lib")

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_handles_whitespace_in_env_values(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        stdout = (
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "  MYVAR  =  some value  \n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
        )
        mock_run.return_value = MagicMock(returncode=0, stdout=stdout)
        with patch.dict(os.environ, {}, clear=True):
            result = probe_msvc_dev_env()
        self.assertEqual(result["MYVAR"], "some value")

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_passes_vscmd_debug_env(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        mock_run.return_value = MagicMock(returncode=0, stdout="no header\n")
        probe_msvc_dev_env()
        call_env = mock_run.call_args[1]["env"]
        self.assertEqual(call_env["VSCMD_DEBUG"], "2")

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[r"C:\VS"])
    @patch("src.tools.visual_studio.os.path.exists", return_value=True)
    @patch("src.tools.visual_studio.subprocess.run")
    def test_handles_equals_in_value(self, mock_run, _mock_exists, _mock_locs, _mock_win):
        stdout = (
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
            "CMAKE_FLAGS=-DFOO=BAR\n"
            "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------\n"
        )
        mock_run.return_value = MagicMock(returncode=0, stdout=stdout)
        with patch.dict(os.environ, {}, clear=True):
            result = probe_msvc_dev_env()
        self.assertEqual(result["CMAKE_FLAGS"], "-DFOO=BAR")

    @patch("src.tools.visual_studio.is_windows", return_value=True)
    @patch("src.tools.visual_studio.get_vs_install_locations", return_value=[])
    def test_returns_none_when_vs_roots_empty(self, _mock_locs, _mock_win):
        self.assertIsNone(probe_msvc_dev_env())


if __name__ == "__main__":
    unittest.main()
