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

"""Tests for system prerequisite detection and reporting."""

import sys
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.tools.prerequisites import (
    Prerequisite,
    PREREQUISITES,
    check_prerequisites,
    report_missing_prerequisites,
    try_install_system_deps,
    _find_c_compiler,
    _find_prerequisite,
    _get_install_hint,
    _detect_package_manager,
    _get_package_names,
    _APT_PACKAGES,
    _BREW_PACKAGES,
)
from src.core.ui import TextUI
from src.__main__ import get_command_line_args


class TestPrerequisiteDataclass(unittest.TestCase):
    """Test the Prerequisite dataclass."""

    def test_default_construction(self):
        p = Prerequisite(name="test", description="A test tool")
        self.assertEqual(p.name, "test")
        self.assertEqual(p.description, "A test tool")
        self.assertTrue(p.required)
        self.assertEqual(p.found_path, "")

    def test_optional_prerequisite(self):
        p = Prerequisite(name="optional", description="Optional", required=False)
        self.assertFalse(p.required)

    def test_install_hints(self):
        p = Prerequisite(
            name="cmake",
            description="CMake",
            install_linux="apt install cmake",
            install_macos="brew install cmake",
            install_windows="choco install cmake",
        )
        self.assertEqual(p.install_linux, "apt install cmake")
        self.assertEqual(p.install_macos, "brew install cmake")
        self.assertEqual(p.install_windows, "choco install cmake")


class TestPrerequisitesList(unittest.TestCase):
    """Test the PREREQUISITES constant."""

    def test_has_required_tools(self):
        names = [p.name for p in PREREQUISITES]
        self.assertIn("cmake", names)
        self.assertIn("git", names)
        self.assertIn("python3", names)
        self.assertIn("cc", names)

    def test_all_have_install_hints(self):
        for p in PREREQUISITES:
            self.assertTrue(p.install_linux, f"{p.name} missing linux install hint")
            self.assertTrue(p.install_macos, f"{p.name} missing macos install hint")
            self.assertTrue(p.install_windows, f"{p.name} missing windows install hint")

    def test_cmake_is_required(self):
        cmake = next(p for p in PREREQUISITES if p.name == "cmake")
        self.assertTrue(cmake.required)

    def test_git_is_required(self):
        git = next(p for p in PREREQUISITES if p.name == "git")
        self.assertTrue(git.required)


class TestFindCCompiler(unittest.TestCase):
    """Test C compiler detection."""

    @patch("src.tools.prerequisites.shutil.which")
    def test_finds_gcc(self, mock_which):
        mock_which.side_effect = lambda name: "/usr/bin/gcc" if name == "gcc" else None
        with patch("src.tools.prerequisites.find_tool", return_value=None):
            result = _find_c_compiler()
        # cc is checked first, then gcc
        self.assertIn("gcc", result)

    @patch("src.tools.prerequisites.shutil.which", return_value=None)
    @patch("src.tools.prerequisites.find_tool", return_value=None)
    def test_returns_none_when_nothing_found(self, mock_find, mock_which):
        result = _find_c_compiler()
        self.assertIsNone(result)

    @patch("src.tools.prerequisites.shutil.which")
    def test_finds_cc(self, mock_which):
        mock_which.side_effect = lambda name: "/usr/bin/cc" if name == "cc" else None
        with patch("src.tools.prerequisites.find_tool", return_value=None):
            result = _find_c_compiler()
        self.assertEqual(result, "/usr/bin/cc")


class TestCheckPrerequisites(unittest.TestCase):
    """Test the check_prerequisites function."""

    @patch("src.tools.prerequisites._find_prerequisite")
    def test_all_found_returns_empty(self, mock_find):
        mock_find.return_value = "/usr/bin/tool"
        ui = TextUI()
        missing = check_prerequisites(build_tests=True, ui=ui)
        self.assertEqual(missing, [])

    @patch("src.tools.prerequisites._find_prerequisite")
    def test_missing_cmake_reported(self, mock_find):
        def side_effect(prereq):
            if prereq.name == "cmake":
                return None
            return "/usr/bin/tool"
        mock_find.side_effect = side_effect
        ui = TextUI()
        missing = check_prerequisites(build_tests=True, ui=ui)
        names = [p.name for p in missing]
        self.assertIn("cmake", names)

    @patch("src.tools.prerequisites._find_prerequisite")
    def test_compiler_check_skipped_with_cc(self, mock_find):
        """When --cc is specified, skip compiler auto-detection."""
        def side_effect(prereq):
            if prereq.name == "cc":
                return None  # Would be missing without --cc
            return "/usr/bin/tool"
        mock_find.side_effect = side_effect
        ui = TextUI()
        missing = check_prerequisites(build_tests=False, cc="/usr/bin/clang", ui=ui)
        names = [p.name for p in missing]
        self.assertNotIn("cc", names)


class TestGetInstallHint(unittest.TestCase):
    """Test platform-specific install hints."""

    def _make_prereq(self):
        return Prerequisite(
            name="test",
            description="Test",
            install_linux="apt install test",
            install_macos="brew install test",
            install_windows="choco install test",
        )

    @patch("src.tools.prerequisites.is_linux", return_value=True)
    @patch("src.tools.prerequisites.is_macos", return_value=False)
    @patch("src.tools.prerequisites.is_windows", return_value=False)
    def test_linux_hint(self, *_):
        hint = _get_install_hint(self._make_prereq())
        self.assertEqual(hint, "apt install test")

    @patch("src.tools.prerequisites.is_linux", return_value=False)
    @patch("src.tools.prerequisites.is_macos", return_value=True)
    @patch("src.tools.prerequisites.is_windows", return_value=False)
    def test_macos_hint(self, *_):
        hint = _get_install_hint(self._make_prereq())
        self.assertEqual(hint, "brew install test")

    @patch("src.tools.prerequisites.is_linux", return_value=False)
    @patch("src.tools.prerequisites.is_macos", return_value=False)
    @patch("src.tools.prerequisites.is_windows", return_value=True)
    def test_windows_hint(self, *_):
        hint = _get_install_hint(self._make_prereq())
        self.assertEqual(hint, "choco install test")


class TestReportMissingPrerequisites(unittest.TestCase):
    """Test error reporting for missing prerequisites."""

    @patch("sys.stderr", new_callable=lambda: MagicMock())
    def test_no_missing_no_output(self, mock_stderr):
        ui = TextUI()
        report_missing_prerequisites([], ui=ui)
        # No error output expected

    def test_required_missing_reports_error(self):
        ui = TextUI()
        missing = [Prerequisite(name="cmake", description="CMake", required=True,
                                install_linux="apt install cmake",
                                install_macos="brew install cmake",
                                install_windows="choco install cmake")]
        # Should not raise - just prints
        report_missing_prerequisites(missing, ui=ui)

    def test_optional_missing_reports_warning(self):
        ui = TextUI()
        missing = [Prerequisite(name="optional", description="Optional", required=False,
                                install_linux="apt install opt",
                                install_macos="brew install opt",
                                install_windows="choco install opt")]
        report_missing_prerequisites(missing, ui=ui)


class TestDetectPackageManager(unittest.TestCase):
    """Test package manager detection."""

    @patch("src.tools.prerequisites.is_linux", return_value=True)
    @patch("src.tools.prerequisites.is_macos", return_value=False)
    @patch("src.tools.prerequisites.shutil.which")
    def test_detects_apt(self, mock_which, *_):
        mock_which.return_value = "/usr/bin/apt-get"
        self.assertEqual(_detect_package_manager(), "apt-get")

    @patch("src.tools.prerequisites.is_linux", return_value=False)
    @patch("src.tools.prerequisites.is_macos", return_value=True)
    @patch("src.tools.prerequisites.shutil.which")
    def test_detects_brew(self, mock_which, *_):
        mock_which.return_value = "/usr/local/bin/brew"
        self.assertEqual(_detect_package_manager(), "brew")

    @patch("src.tools.prerequisites.is_linux", return_value=False)
    @patch("src.tools.prerequisites.is_macos", return_value=False)
    def test_returns_none_on_windows(self, *_):
        self.assertIsNone(_detect_package_manager())


class TestGetPackageNames(unittest.TestCase):
    """Test mapping prerequisites to package names."""

    def test_apt_packages(self):
        prereqs = [
            Prerequisite(name="cmake", description="CMake"),
            Prerequisite(name="git", description="Git"),
        ]
        packages = _get_package_names(prereqs, "apt-get")
        self.assertEqual(packages, ["cmake", "git"])

    def test_brew_packages(self):
        prereqs = [
            Prerequisite(name="cmake", description="CMake"),
            Prerequisite(name="git", description="Git"),
        ]
        packages = _get_package_names(prereqs, "brew")
        self.assertEqual(packages, ["cmake", "git"])

    def test_brew_skips_cc(self):
        """cc maps to None for brew (needs xcode-select, not brew)."""
        prereqs = [Prerequisite(name="cc", description="C compiler")]
        packages = _get_package_names(prereqs, "brew")
        self.assertEqual(packages, [])

    def test_apt_cc_maps_to_build_essential(self):
        prereqs = [Prerequisite(name="cc", description="C compiler")]
        packages = _get_package_names(prereqs, "apt-get")
        self.assertEqual(packages, ["build-essential"])


class TestTryInstallSystemDeps(unittest.TestCase):
    """Test automatic dependency installation."""

    @patch("src.tools.prerequisites._detect_package_manager", return_value=None)
    def test_no_pkg_manager_returns_all_missing(self, _):
        missing = [Prerequisite(name="cmake", description="CMake")]
        ui = TextUI()
        result = try_install_system_deps(missing, ui=ui)
        self.assertEqual(result, missing)

    def test_empty_missing_returns_empty(self):
        ui = TextUI()
        result = try_install_system_deps([], ui=ui)
        self.assertEqual(result, [])

    @patch("src.tools.prerequisites._detect_package_manager", return_value="apt-get")
    @patch("src.tools.prerequisites.subprocess.run")
    @patch("src.tools.prerequisites._find_prerequisite", return_value="/usr/bin/cmake")
    def test_successful_install(self, mock_find, mock_run, mock_pkg):
        missing = [Prerequisite(name="cmake", description="CMake")]
        ui = TextUI()
        result = try_install_system_deps(missing, ui=ui)
        self.assertEqual(result, [])
        mock_run.assert_called_once()
        cmd = mock_run.call_args[0][0]
        self.assertIn("cmake", cmd)


class TestCLIInstallSystemDeps(unittest.TestCase):
    """Test --install-system-deps CLI argument."""

    def test_install_system_deps_flag(self):
        args = get_command_line_args(["--install-system-deps"])
        self.assertTrue(args["install_system_deps"])

    def test_install_system_deps_not_set_by_default(self):
        args = get_command_line_args([])
        self.assertFalse(args.get("install_system_deps"))


class TestOptionsInstallSystemDeps(unittest.TestCase):
    """Test install_system_deps field in InstallOptions."""

    def test_default_false(self):
        from src.core.options import InstallOptions
        opts = InstallOptions()
        self.assertFalse(opts.install_system_deps)

    def test_can_set_true(self):
        from src.core.options import InstallOptions
        opts = InstallOptions(install_system_deps=True)
        self.assertTrue(opts.install_system_deps)


if __name__ == "__main__":
    unittest.main()
