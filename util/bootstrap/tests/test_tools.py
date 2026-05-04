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

"""Tests for tools/compilers.py, tools/ninja.py, tools/detection.py."""

import io
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch, MagicMock, call

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.tools.compilers import check_compiler, probe_compilers, sanitizer_flag_name
from src.tools.detection import is_tool_executable, find_tool
from src.tools.ninja import get_ninja_asset_name, install_ninja
from src.core.ui import TextUI


def _make_ui():
    """Create a plain TextUI for testing (no color, no emoji)."""
    return TextUI()


# ── is_tool_executable ─────────────────────────────────────────────

class TestIsToolExecutable(unittest.TestCase):

    def test_nonexistent_path_returns_false(self):
        self.assertFalse(is_tool_executable("/no/such/file"))

    def test_directory_returns_false(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertFalse(is_tool_executable(tmp))

    @patch("src.tools.detection.is_windows", return_value=False)
    def test_executable_file_unix(self, _):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"#!/bin/sh\n")
            path = f.name
        try:
            os.chmod(path, 0o755)
            self.assertTrue(is_tool_executable(path))
        finally:
            os.unlink(path)

    @patch("src.tools.detection.is_windows", return_value=False)
    def test_non_executable_file_unix(self, _):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            path = f.name
        try:
            os.chmod(path, 0o644)
            self.assertFalse(is_tool_executable(path))
        finally:
            os.unlink(path)

    @patch("src.tools.detection.is_windows", return_value=True)
    def test_windows_exe_extension(self, _):
        with tempfile.NamedTemporaryFile(suffix=".exe", delete=False) as f:
            path = f.name
        try:
            self.assertTrue(is_tool_executable(path))
        finally:
            os.unlink(path)

    @patch("src.tools.detection.is_windows", return_value=True)
    def test_windows_bat_extension(self, _):
        with tempfile.NamedTemporaryFile(suffix=".bat", delete=False) as f:
            path = f.name
        try:
            self.assertTrue(is_tool_executable(path))
        finally:
            os.unlink(path)

    @patch("src.tools.detection.is_windows", return_value=True)
    def test_windows_no_extension_returns_false(self, _):
        with tempfile.NamedTemporaryFile(suffix=".txt", delete=False) as f:
            path = f.name
        try:
            self.assertFalse(is_tool_executable(path))
        finally:
            os.unlink(path)


# ── find_tool ──────────────────────────────────────────────────────

class TestFindTool(unittest.TestCase):

    @patch("src.tools.detection.shutil.which", return_value="/usr/bin/cmake")
    def test_found_in_path(self, mock_which):
        result = find_tool("cmake")
        self.assertEqual(result, "/usr/bin/cmake")

    @patch("src.tools.detection.is_windows", return_value=False)
    @patch("src.tools.detection.shutil.which", return_value=None)
    def test_not_found_returns_none(self, mock_which, _):
        result = find_tool("nonexistent_tool_xyz")
        self.assertIsNone(result)

    @patch("src.tools.detection.shutil.which", return_value=None)
    @patch("src.tools.detection.is_tool_executable", return_value=True)
    @patch("src.tools.detection.os.path.exists", return_value=True)
    @patch("src.tools.detection.os.path.isdir", return_value=False)
    def test_found_via_env_var_executable(self, mock_isdir, mock_exists, mock_exec, mock_which):
        with patch.dict(os.environ, {"CMAKE_ROOT": "/opt/cmake/bin/cmake"}):
            result = find_tool("cmake")
            self.assertEqual(result, "/opt/cmake/bin/cmake")

    @patch("src.tools.detection.shutil.which", return_value=None)
    @patch("src.tools.detection.is_windows", return_value=False)
    @patch("src.tools.detection.is_tool_executable")
    @patch("src.tools.detection.os.path.exists", return_value=True)
    @patch("src.tools.detection.os.path.isdir")
    def test_found_via_env_var_dir_with_tool(self, mock_isdir, mock_exists, mock_exec, mock_win, mock_which):
        def isdir_side(p):
            return p == "/opt/cmake"
        mock_isdir.side_effect = isdir_side
        def exec_side(p):
            return p == "/opt/cmake/cmake"
        mock_exec.side_effect = exec_side
        with patch.dict(os.environ, {"CMAKE_ROOT": "/opt/cmake"}):
            result = find_tool("cmake")
            self.assertEqual(result, "/opt/cmake/cmake")

    @patch("src.tools.detection.shutil.which", return_value=None)
    @patch("src.tools.detection.is_windows", return_value=False)
    @patch("src.tools.detection.is_tool_executable")
    @patch("src.tools.detection.os.path.exists", return_value=True)
    @patch("src.tools.detection.os.path.isdir")
    def test_found_via_env_var_dir_with_bin_subdir(self, mock_isdir, mock_exists, mock_exec, mock_win, mock_which):
        def isdir_side(p):
            return p == "/opt/cmake"
        mock_isdir.side_effect = isdir_side
        def exec_side(p):
            return p == "/opt/cmake/bin/cmake"
        mock_exec.side_effect = exec_side
        with patch.dict(os.environ, {"CMAKE_ROOT": "/opt/cmake"}):
            result = find_tool("cmake")
            self.assertEqual(result, "/opt/cmake/bin/cmake")

    def test_python_returns_sys_executable(self):
        with patch("src.tools.detection.shutil.which", return_value=None):
            with patch("src.tools.detection.is_windows", return_value=False):
                result = find_tool("python")
                self.assertEqual(result, sys.executable)

    @patch("src.tools.detection.is_windows", return_value=True)
    @patch("src.tools.detection.shutil.which", return_value=None)
    def test_windows_vs_tool(self, mock_which, mock_win):
        mock_mod = MagicMock()
        mock_mod.find_vs_tool = MagicMock(return_value="C:\\VS\\cmake.exe")
        with patch.dict("sys.modules", {"src.tools.visual_studio": mock_mod}):
            result = find_tool("cmake")
            self.assertEqual(result, "C:\\VS\\cmake.exe")


# ── check_compiler ─────────────────────────────────────────────────

class TestCheckCompiler(unittest.TestCase):

    def test_empty_path_returns_empty(self):
        self.assertEqual(check_compiler(""), "")

    def test_empty_none_returns_empty(self):
        self.assertEqual(check_compiler(None), "")

    @patch("src.tools.compilers.is_tool_executable", return_value=True)
    def test_absolute_path_found(self, mock_exec):
        result = check_compiler("/usr/bin/gcc")
        self.assertEqual(result, "/usr/bin/gcc")
        mock_exec.assert_called_once_with("/usr/bin/gcc")

    @patch("src.tools.compilers.is_tool_executable", return_value=False)
    def test_absolute_path_not_executable_raises(self, mock_exec):
        with self.assertRaises(FileNotFoundError):
            check_compiler("/usr/bin/gcc")

    @patch("src.tools.compilers.shutil.which", return_value="/usr/bin/gcc")
    @patch("src.tools.compilers.is_tool_executable", return_value=True)
    def test_relative_path_resolved(self, mock_exec, mock_which):
        result = check_compiler("gcc")
        self.assertEqual(result, "/usr/bin/gcc")

    @patch("src.tools.compilers.shutil.which", return_value=None)
    def test_relative_path_not_found_raises(self, mock_which):
        with self.assertRaises(FileNotFoundError) as ctx:
            check_compiler("gcc", "cc")
        self.assertIn("cc executable", str(ctx.exception))

    @patch("src.tools.compilers.shutil.which", return_value="/usr/bin/g++")
    @patch("src.tools.compilers.is_tool_executable", return_value=False)
    def test_resolved_but_not_executable_raises(self, mock_exec, mock_which):
        with self.assertRaises(FileNotFoundError):
            check_compiler("g++", "cxx")


# ── probe_compilers ────────────────────────────────────────────────

class TestProbeCompilers(unittest.TestCase):

    def test_dry_run_returns_empty_dict(self):
        ui = _make_ui()
        with tempfile.TemporaryDirectory() as tmp:
            probe_dir = os.path.join(tmp, "probe")
            captured = io.StringIO()
            with patch("sys.stdout", captured):
                result = probe_compilers(
                    cmake_path="/usr/bin/cmake",
                    probe_dir=probe_dir,
                    cc="/usr/bin/gcc",
                    cxx="/usr/bin/g++",
                    dry_run=True,
                    ui=ui,
                )
            self.assertEqual(result, {})

    @patch("src.tools.compilers.subprocess.run")
    def test_success_parses_output(self, mock_run):
        ui = _make_ui()
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="-- CMAKE_CXX_COMPILER=/usr/bin/g++\n-- CMAKE_CXX_COMPILER_ID=GNU\n-- CMAKE_CXX_COMPILER_VERSION=15.1\n-- CMAKE_C_COMPILER=/usr/bin/gcc\n",
            stderr="",
        )
        with tempfile.TemporaryDirectory() as tmp:
            probe_dir = os.path.join(tmp, "probe")
            result = probe_compilers(
                cmake_path="/usr/bin/cmake",
                probe_dir=probe_dir,
                cc="/usr/bin/gcc",
                cxx="/usr/bin/g++",
                dry_run=False,
                ui=ui,
            )
        self.assertEqual(result.get("CMAKE_CXX_COMPILER"), "/usr/bin/g++")
        self.assertEqual(result.get("CMAKE_CXX_COMPILER_ID"), "GNU")
        self.assertEqual(result.get("CMAKE_CXX_COMPILER_VERSION"), "15.1")
        self.assertEqual(result.get("CMAKE_C_COMPILER"), "/usr/bin/gcc")

    @patch("src.tools.compilers.subprocess.run")
    def test_failure_raises_runtime_error(self, mock_run):
        ui = _make_ui()
        mock_run.return_value = MagicMock(
            returncode=1,
            stdout="error output",
            stderr="cmake error",
        )
        with tempfile.TemporaryDirectory() as tmp:
            probe_dir = os.path.join(tmp, "probe")
            with self.assertRaises(RuntimeError) as ctx:
                probe_compilers(
                    cmake_path="/usr/bin/cmake",
                    probe_dir=probe_dir,
                    dry_run=False,
                    ui=ui,
                )
            self.assertIn("CMake failed", str(ctx.exception))

    @patch("src.tools.compilers.subprocess.run")
    def test_no_cc_cxx_args(self, mock_run):
        ui = _make_ui()
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="-- CMAKE_CXX_COMPILER_ID=Clang\n",
            stderr="",
        )
        with tempfile.TemporaryDirectory() as tmp:
            probe_dir = os.path.join(tmp, "probe")
            result = probe_compilers(
                cmake_path="/usr/bin/cmake",
                probe_dir=probe_dir,
                dry_run=False,
                ui=ui,
            )
        cmd = mock_run.call_args[0][0]
        self.assertNotIn("-DCMAKE_C_COMPILER=", " ".join(cmd))
        self.assertNotIn("-DCMAKE_CXX_COMPILER=", " ".join(cmd))

    def test_default_ui_used_when_none(self):
        with tempfile.TemporaryDirectory() as tmp:
            probe_dir = os.path.join(tmp, "probe")
            captured = io.StringIO()
            with patch("sys.stdout", captured):
                result = probe_compilers(
                    cmake_path="/usr/bin/cmake",
                    probe_dir=probe_dir,
                    dry_run=True,
                    ui=None,
                )
            self.assertEqual(result, {})


# ── sanitizer_flag_name ────────────────────────────────────────────

class TestSanitizerFlagName(unittest.TestCase):

    def test_asan(self):
        self.assertEqual(sanitizer_flag_name("asan"), "address")

    def test_ubsan(self):
        self.assertEqual(sanitizer_flag_name("ubsan"), "undefined")

    def test_msan(self):
        self.assertEqual(sanitizer_flag_name("msan"), "memory")

    def test_tsan(self):
        self.assertEqual(sanitizer_flag_name("tsan"), "thread")

    def test_passthrough(self):
        self.assertEqual(sanitizer_flag_name("address"), "address")

    def test_unknown_passthrough(self):
        self.assertEqual(sanitizer_flag_name("custom"), "custom")

    def test_case_insensitive(self):
        self.assertEqual(sanitizer_flag_name("ASAN"), "address")
        self.assertEqual(sanitizer_flag_name("Ubsan"), "undefined")


# ── get_ninja_asset_name ───────────────────────────────────────────

class TestGetNinjaAssetName(unittest.TestCase):

    @patch("src.tools.ninja.platform.system", return_value="Linux")
    @patch("src.tools.ninja.platform.machine", return_value="x86_64")
    def test_linux_x86(self, _m, _s):
        self.assertEqual(get_ninja_asset_name(), "ninja-linux.zip")

    @patch("src.tools.ninja.platform.system", return_value="Linux")
    @patch("src.tools.ninja.platform.machine", return_value="aarch64")
    def test_linux_arm(self, _m, _s):
        self.assertEqual(get_ninja_asset_name(), "ninja-linux-aarch64.zip")

    @patch("src.tools.ninja.platform.system", return_value="Linux")
    @patch("src.tools.ninja.platform.machine", return_value="arm64")
    def test_linux_arm64(self, _m, _s):
        self.assertEqual(get_ninja_asset_name(), "ninja-linux-aarch64.zip")

    @patch("src.tools.ninja.platform.system", return_value="Darwin")
    @patch("src.tools.ninja.platform.machine", return_value="x86_64")
    def test_darwin(self, _m, _s):
        self.assertEqual(get_ninja_asset_name(), "ninja-mac.zip")

    @patch("src.tools.ninja.platform.system", return_value="Windows")
    @patch("src.tools.ninja.platform.machine", return_value="AMD64")
    def test_windows_x86(self, _m, _s):
        self.assertEqual(get_ninja_asset_name(), "ninja-win.zip")

    @patch("src.tools.ninja.platform.system", return_value="Windows")
    @patch("src.tools.ninja.platform.machine", return_value="arm64")
    def test_windows_arm(self, _m, _s):
        self.assertEqual(get_ninja_asset_name(), "ninja-winarm64.zip")

    @patch("src.tools.ninja.platform.system", return_value="FreeBSD")
    @patch("src.tools.ninja.platform.machine", return_value="x86_64")
    def test_unsupported_returns_none(self, _m, _s):
        self.assertIsNone(get_ninja_asset_name())


# ── install_ninja ──────────────────────────────────────────────────

class TestInstallNinja(unittest.TestCase):

    def test_user_specified_absolute_path_found(self):
        ui = _make_ui()
        with tempfile.TemporaryDirectory() as tmp:
            ninja_path = os.path.join(tmp, "ninja")
            with open(ninja_path, "w") as f:
                f.write("#!/bin/sh\n")
            os.chmod(ninja_path, 0o755)
            result = install_ninja(
                source_dir=tmp,
                preset="test",
                ninja_path=ninja_path,
                ui=ui,
            )
            self.assertEqual(result, ninja_path)

    def test_user_specified_path_not_executable_raises(self):
        ui = _make_ui()
        with tempfile.TemporaryDirectory() as tmp:
            result_path = os.path.join(tmp, "ninja_bad")
            with open(result_path, "w") as f:
                f.write("")
            os.chmod(result_path, 0o644)
            with self.assertRaises(FileNotFoundError):
                install_ninja(
                    source_dir=tmp,
                    preset="test",
                    ninja_path=result_path,
                    ui=ui,
                )

    @patch("src.tools.ninja.find_tool", return_value="/usr/bin/ninja")
    def test_user_specified_relative_resolved(self, mock_find):
        ui = _make_ui()
        with patch("src.tools.ninja.is_tool_executable", return_value=True):
            result = install_ninja(
                source_dir="/tmp/src",
                preset="test",
                ninja_path="ninja",
                ui=ui,
            )
        self.assertEqual(result, "/usr/bin/ninja")

    @patch("src.tools.ninja.find_tool", return_value="/usr/bin/ninja")
    def test_found_in_path(self, mock_find):
        ui = _make_ui()
        result = install_ninja(
            source_dir="/tmp/src",
            preset="test",
            ninja_path=None,
            ui=ui,
        )
        self.assertEqual(result, "/usr/bin/ninja")

    @patch("src.tools.ninja.find_tool", return_value=None)
    def test_already_downloaded_reuses(self, mock_find):
        ui = _make_ui()
        with tempfile.TemporaryDirectory() as tmp:
            # Create the expected ninja executable
            install_dir = os.path.join(tmp, "build", "third-party", "install", "test", "ninja")
            os.makedirs(install_dir, exist_ok=True)
            ninja_exe = os.path.join(install_dir, "ninja")
            with open(ninja_exe, "w") as f:
                f.write("#!/bin/sh\n")
            os.chmod(ninja_exe, 0o755)
            result = install_ninja(
                source_dir=tmp,
                preset="test",
                ninja_path=None,
                ui=ui,
            )
            self.assertEqual(result, ninja_exe)

    @patch("src.tools.ninja.find_tool", return_value=None)
    @patch("src.tools.ninja.is_tool_executable", return_value=False)
    @patch("src.tools.ninja.get_ninja_asset_name", return_value="ninja-linux.zip")
    @patch("src.tools.ninja.is_windows", return_value=False)
    def test_dry_run_prints_commands(self, mock_win, mock_asset, mock_exec, mock_find):
        ui = _make_ui()
        captured = io.StringIO()
        with tempfile.TemporaryDirectory() as tmp:
            with patch("sys.stdout", captured):
                result = install_ninja(
                    source_dir=tmp,
                    preset="test",
                    dry_run=True,
                    ui=ui,
                )
        output = captured.getvalue()
        self.assertIn("curl", output)
        self.assertIn("unzip", output)
        self.assertIn("chmod", output)
        self.assertIsNotNone(result)

    @patch("src.tools.ninja.find_tool", return_value=None)
    @patch("src.tools.ninja.is_tool_executable", return_value=False)
    @patch("src.tools.ninja.get_ninja_asset_name", return_value=None)
    def test_unsupported_platform_returns_none(self, mock_asset, mock_exec, mock_find):
        ui = _make_ui()
        with tempfile.TemporaryDirectory() as tmp:
            result = install_ninja(
                source_dir=tmp,
                preset="test",
                ui=ui,
            )
        self.assertIsNone(result)

    @patch("src.tools.ninja.find_tool", return_value=None)
    @patch("src.tools.ninja.is_tool_executable", return_value=False)
    @patch("src.tools.ninja.get_ninja_asset_name", return_value="ninja-linux.zip")
    @patch("src.tools.ninja.is_windows", return_value=False)
    @patch("src.tools.ninja.urllib.request.urlopen")
    @patch("src.tools.ninja.urllib.request.urlretrieve")
    def test_download_and_extract(self, mock_retrieve, mock_urlopen, mock_win, mock_asset, mock_exec, mock_find):
        ui = _make_ui()
        api_response = MagicMock()
        api_response.__enter__ = MagicMock(return_value=api_response)
        api_response.__exit__ = MagicMock(return_value=False)
        api_response.read = MagicMock(return_value=json.dumps({
            "assets": [{"name": "ninja-linux.zip", "browser_download_url": "https://example.com/ninja.zip"}]
        }).encode())

        # Make urlopen return the mock
        mock_urlopen.return_value = api_response

        with tempfile.TemporaryDirectory() as tmp:
            # Pre-create the zip that urlretrieve would download
            install_dir = os.path.join(tmp, "build", "third-party", "install", "test", "ninja")
            download_dir = os.path.join(tmp, "build", "third-party", "source", "ninja")
            os.makedirs(install_dir, exist_ok=True)
            os.makedirs(download_dir, exist_ok=True)
            zip_path = os.path.join(download_dir, "ninja-linux.zip")

            def fake_retrieve(url, path):
                with zipfile.ZipFile(path, 'w') as zf:
                    zf.writestr("ninja", "#!/bin/sh\necho ninja")

            mock_retrieve.side_effect = fake_retrieve

            result = install_ninja(
                source_dir=tmp,
                preset="test",
                ui=ui,
            )
            self.assertIsNotNone(result)
            self.assertIn("ninja", result)

    @patch("src.tools.ninja.find_tool", return_value=None)
    @patch("src.tools.ninja.is_tool_executable", return_value=False)
    @patch("src.tools.ninja.get_ninja_asset_name", return_value="ninja-linux.zip")
    @patch("src.tools.ninja.is_windows", return_value=False)
    @patch("src.tools.ninja.urllib.request.urlopen")
    def test_no_matching_asset_returns_none(self, mock_urlopen, mock_win, mock_asset, mock_exec, mock_find):
        ui = _make_ui()
        api_response = MagicMock()
        api_response.__enter__ = MagicMock(return_value=api_response)
        api_response.__exit__ = MagicMock(return_value=False)
        api_response.read = MagicMock(return_value=json.dumps({
            "assets": [{"name": "ninja-OTHER.zip", "browser_download_url": "https://example.com/other.zip"}]
        }).encode())
        mock_urlopen.return_value = api_response

        with tempfile.TemporaryDirectory() as tmp:
            result = install_ninja(
                source_dir=tmp,
                preset="test",
                ui=ui,
            )
        self.assertIsNone(result)


if __name__ == "__main__":
    unittest.main()
