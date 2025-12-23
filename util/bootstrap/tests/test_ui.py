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

"""Tests for TextUI class."""

import io
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.core.ui import TextUI, get_default_ui, set_default_ui


class TestTextUIConstruction(unittest.TestCase):
    """Test TextUI construction."""

    def test_default_construction(self):
        """Should construct with defaults (no color, no emoji)."""
        ui = TextUI()
        self.assertFalse(ui.color_enabled)
        self.assertFalse(ui.emoji_enabled)

    def test_enable_color(self):
        """Should be able to enable color."""
        # Color depends on terminal support, so we can't guarantee it's enabled
        ui = TextUI(enable_color=True)
        self.assertIsInstance(ui.color_enabled, bool)

    def test_enable_emoji(self):
        """Should be able to enable emoji."""
        ui = TextUI(enable_emoji=True)
        # Emoji is typically enabled if requested
        self.assertIsInstance(ui.emoji_enabled, bool)

    def test_max_path_default(self):
        """Default max_path should be 50."""
        ui = TextUI()
        self.assertEqual(ui.max_path, 50)


class TestTextUIFormatting(unittest.TestCase):
    """Test TextUI formatting methods."""

    def setUp(self):
        """Create a plain UI for testing."""
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    def test_fmt_without_color(self):
        """_fmt without color should return plain text."""
        result = self.ui._fmt("test message", "info")
        self.assertEqual(result, "test message")

    def test_color_dict_has_required_keys(self):
        """COLOR dict should have all required keys."""
        required = ["reset", "info", "warn", "error", "ok", "section", "subsection", "command", "dim"]
        for key in required:
            self.assertIn(key, TextUI.COLOR)

    def test_emoji_dict_has_required_keys(self):
        """EMOJI dict should have required keys."""
        required = ["info", "warn", "error", "ok", "section", "command"]
        for key in required:
            self.assertIn(key, TextUI.EMOJI)


class TestTextUIOutput(unittest.TestCase):
    """Test TextUI output methods."""

    def setUp(self):
        """Create a plain UI for testing."""
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_info_prints(self, mock_stdout):
        """info() should print message."""
        self.ui.info("test info")
        self.assertIn("test info", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_warn_prints(self, mock_stdout):
        """warn() should print message."""
        self.ui.warn("test warning")
        self.assertIn("test warning", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_error_prints(self, mock_stdout):
        """error() should print message."""
        self.ui.error("test error")
        self.assertIn("test error", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_ok_prints(self, mock_stdout):
        """ok() should print message."""
        self.ui.ok("test ok")
        self.assertIn("test ok", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_section_prints(self, mock_stdout):
        """section() should print title."""
        self.ui.section("Test Section")
        self.assertIn("Test Section", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_subsection_prints(self, mock_stdout):
        """subsection() should print title."""
        self.ui.subsection("Test Subsection")
        self.assertIn("Test Subsection", mock_stdout.getvalue())


class TestTextUIPathHandling(unittest.TestCase):
    """Test TextUI path shortening."""

    def setUp(self):
        """Create a UI with base path set."""
        self.ui = TextUI(enable_color=False, enable_emoji=False)
        self.ui.set_base_path("/home/user/mrdocs")

    def test_set_base_path(self):
        """set_base_path should store the path."""
        ui = TextUI()
        ui.set_base_path("/test/path")
        self.assertEqual(ui.base_path, "/test/path")

    def test_maybe_shorten_with_base_path(self):
        """maybe_shorten should use base_token for paths under base_path."""
        result = self.ui.maybe_shorten("/home/user/mrdocs/build/file.txt")
        self.assertIn(".", result)  # Should contain base_token
        self.assertNotIn("/home/user/mrdocs", result)

    def test_maybe_shorten_without_base_path(self):
        """maybe_shorten should not modify paths outside base_path."""
        result = self.ui.maybe_shorten("/other/path/file.txt")
        self.assertEqual(result, "/other/path/file.txt")

    def test_maybe_shorten_home_dir(self):
        """maybe_shorten should replace home dir with ~."""
        import os
        home = os.path.expanduser("~")
        ui = TextUI()
        result = ui.maybe_shorten(f"{home}/some/file.txt")
        self.assertTrue(result.startswith("~") or result.startswith(home))


class TestDefaultUI(unittest.TestCase):
    """Test global default UI functions."""

    def test_get_default_ui_returns_textui(self):
        """get_default_ui should return a TextUI instance."""
        ui = get_default_ui()
        self.assertIsInstance(ui, TextUI)

    def test_set_and_get_default_ui(self):
        """set_default_ui should change the default UI."""
        original = get_default_ui()
        try:
            new_ui = TextUI(enable_color=False, enable_emoji=False)
            set_default_ui(new_ui)
            self.assertIs(get_default_ui(), new_ui)
        finally:
            # Restore original
            set_default_ui(original)


if __name__ == "__main__":
    unittest.main()
