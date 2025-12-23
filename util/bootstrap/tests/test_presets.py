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

"""Tests for CMake presets generation."""

import sys
import unittest

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.presets.generator import (
    get_host_system_name,
    get_parent_preset_name,
    get_display_name,
    normalize_preset_value,
)


class TestGetHostSystemName(unittest.TestCase):
    """Test get_host_system_name function."""

    def test_returns_tuple(self):
        """Should return a tuple of two strings."""
        result = get_host_system_name()
        self.assertIsInstance(result, tuple)
        self.assertEqual(len(result), 2)

    def test_first_element_is_system_name(self):
        """First element should be a valid system name."""
        system_name, _ = get_host_system_name()
        self.assertIn(system_name, ["Windows", "Linux", "Darwin"])

    def test_second_element_is_display_name(self):
        """Second element should be a display name."""
        _, display_name = get_host_system_name()
        self.assertIn(display_name, ["Windows", "Linux", "macOS"])

    def test_darwin_maps_to_macos(self):
        """Darwin system should have macOS display name."""
        system_name, display_name = get_host_system_name()
        if system_name == "Darwin":
            self.assertEqual(display_name, "macOS")


class TestGetParentPresetName(unittest.TestCase):
    """Test get_parent_preset_name function."""

    def test_debug_returns_debug(self):
        """Debug build type should return debug preset."""
        self.assertEqual(get_parent_preset_name("Debug"), "debug")

    def test_debug_case_insensitive(self):
        """Should be case insensitive."""
        self.assertEqual(get_parent_preset_name("debug"), "debug")
        self.assertEqual(get_parent_preset_name("DEBUG"), "debug")

    def test_release_returns_release(self):
        """Release build type should return release preset."""
        self.assertEqual(get_parent_preset_name("Release"), "release")

    def test_relwithdebinfo_returns_relwithdebinfo(self):
        """RelWithDebInfo should return relwithdebinfo preset."""
        self.assertEqual(get_parent_preset_name("RelWithDebInfo"), "relwithdebinfo")

    def test_minsizerel_returns_release(self):
        """MinSizeRel should return release preset."""
        self.assertEqual(get_parent_preset_name("MinSizeRel"), "release")

    def test_debugfast_returns_debug(self):
        """DebugFast variants should return debug preset."""
        self.assertEqual(get_parent_preset_name("debugfast"), "debug")
        self.assertEqual(get_parent_preset_name("debug-fast"), "debug")


class TestGetDisplayName(unittest.TestCase):
    """Test get_display_name function."""

    def test_basic_display_name(self):
        """Should create basic display name."""
        name = get_display_name("Release", "Linux")
        self.assertIn("Release", name)
        self.assertIn("Linux", name)

    def test_with_compiler(self):
        """Should include compiler in display name."""
        name = get_display_name("Debug", "macOS", cc="/usr/bin/clang")
        self.assertIn("Debug", name)
        self.assertIn("macOS", name)
        self.assertIn("clang", name)

    def test_with_sanitizer(self):
        """Should include sanitizer in display name."""
        name = get_display_name("Debug", "Linux", sanitizer="address")
        self.assertIn("address", name)

    def test_debugfast_display(self):
        """DebugFast should display as Debug (fast)."""
        name = get_display_name("debugfast", "Linux")
        self.assertIn("Debug (fast)", name)


class TestNormalizePresetValue(unittest.TestCase):
    """Test normalize_preset_value function."""

    def test_source_dir_replacement(self):
        """Should replace source_dir with ${sourceDir}."""
        result = normalize_preset_value(
            "/home/user/mrdocs/build",
            source_dir="/home/user/mrdocs"
        )
        self.assertEqual(result, "${sourceDir}/build")

    def test_home_dir_replacement(self):
        """Should replace home_dir with $env{HOME}."""
        result = normalize_preset_value(
            "/home/user/.local/bin",
            source_dir="/other/path",
            home_dir="/home/user"
        )
        self.assertEqual(result, "$env{HOME}/.local/bin")

    def test_no_replacement_needed(self):
        """Should not modify paths that don't match."""
        result = normalize_preset_value(
            "/usr/bin/cmake",
            source_dir="/home/user/mrdocs"
        )
        self.assertEqual(result, "/usr/bin/cmake")

    def test_non_string_passthrough(self):
        """Non-string values should pass through unchanged."""
        result = normalize_preset_value(42, source_dir="/path")
        self.assertEqual(result, 42)

    def test_semicolon_separated_paths(self):
        """Should handle semicolon-separated paths."""
        result = normalize_preset_value(
            "/home/user/mrdocs/a;/home/user/mrdocs/b",
            source_dir="/home/user/mrdocs"
        )
        self.assertEqual(result, "${sourceDir}/a;${sourceDir}/b")


if __name__ == "__main__":
    unittest.main()
