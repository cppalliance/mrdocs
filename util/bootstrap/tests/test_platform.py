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

"""Tests for platform detection utilities."""

import sys
import unittest

# Add src to path for imports
sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.core.platform import (
    is_windows,
    is_linux,
    is_macos,
    get_os_name,
)


class TestPlatformDetection(unittest.TestCase):
    """Test platform detection functions."""

    def test_os_detection_mutual_exclusion(self):
        """At most one of is_windows/is_linux/is_macos should be True."""
        detected = [is_windows(), is_linux(), is_macos()]
        # Should have at most one True value
        self.assertLessEqual(sum(detected), 1)

    def test_get_os_name_returns_string(self):
        """get_os_name should return a non-empty string."""
        name = get_os_name()
        self.assertIsInstance(name, str)
        self.assertTrue(len(name) > 0)

    def test_get_os_name_matches_detection(self):
        """get_os_name should match the is_* functions."""
        name = get_os_name()
        if is_windows():
            self.assertEqual(name, "windows")
        elif is_linux():
            self.assertEqual(name, "linux")
        elif is_macos():
            self.assertEqual(name, "macos")


if __name__ == "__main__":
    unittest.main()
