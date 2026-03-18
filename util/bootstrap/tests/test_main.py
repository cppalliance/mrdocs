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

"""Tests for __main__.py CLI entry point and remaining ui.py paths."""

import io
import os
import sys
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.__main__ import build_arg_parser, get_command_line_args, main
from src.core.ui import TextUI, get_default_ui, set_default_ui


# ────────────────────────────────────────────────────────────────────
# build_arg_parser
# ────────────────────────────────────────────────────────────────────

class TestBuildArgParser(unittest.TestCase):
    """Verify that all expected arguments are registered."""

    def setUp(self):
        self.parser = build_arg_parser()

    def test_version_action(self):
        """--version should trigger version action."""
        with self.assertRaises(SystemExit) as ctx:
            self.parser.parse_args(["--version"])
        self.assertEqual(ctx.exception.code, 0)

    def test_build_type_choices(self):
        """--build-type should accept valid build types."""
        args = self.parser.parse_args(["--build-type", "Debug"])
        self.assertEqual(args.build_type, "Debug")

    def test_build_type_invalid(self):
        """--build-type with invalid value should error."""
        with self.assertRaises(SystemExit):
            self.parser.parse_args(["--build-type", "InvalidType"])

    def test_compiler_options(self):
        """--cc and --cxx should be accepted."""
        args = self.parser.parse_args(["--cc", "/usr/bin/gcc", "--cxx", "/usr/bin/g++"])
        self.assertEqual(args.cc, "/usr/bin/gcc")
        self.assertEqual(args.cxx, "/usr/bin/g++")

    def test_tool_paths(self):
        """Tool path options should be accepted."""
        args = self.parser.parse_args([
            "--cmake-path", "/usr/bin/cmake",
            "--ninja-path", "/usr/bin/ninja",
            "--git-path", "/usr/bin/git",
            "--python-path", "/usr/bin/python3",
            "--java-path", "/usr/bin/java",
        ])
        self.assertEqual(args.cmake_path, "/usr/bin/cmake")
        self.assertEqual(args.ninja_path, "/usr/bin/ninja")
        self.assertEqual(args.git_path, "/usr/bin/git")
        self.assertEqual(args.python_path, "/usr/bin/python3")
        self.assertEqual(args.java_path, "/usr/bin/java")

    def test_directory_options(self):
        """Directory options should be accepted."""
        args = self.parser.parse_args([
            "--source-dir", "/src",
            "--build-dir", "/build",
            "--install-dir", "/install",
        ])
        self.assertEqual(args.source_dir, "/src")
        self.assertEqual(args.build_dir, "/build")
        self.assertEqual(args.install_dir, "/install")

    def test_behavior_options(self):
        """Behavior flags should be accepted."""
        args = self.parser.parse_args(["--yes", "--dry-run", "--verbose", "--debug", "--plain"])
        self.assertTrue(args.non_interactive)
        self.assertTrue(args.dry_run)
        self.assertTrue(args.verbose)
        self.assertTrue(args.debug)
        self.assertTrue(args.plain_ui)

    def test_short_yes(self):
        """-y should set non_interactive."""
        args = self.parser.parse_args(["-y"])
        self.assertTrue(args.non_interactive)

    def test_dependency_options(self):
        """Dependency flags should be accepted."""
        args = self.parser.parse_args(["--clean", "--force", "--skip-build", "--list-recipes"])
        self.assertTrue(args.clean)
        self.assertTrue(args.force)
        self.assertTrue(args.skip_build)
        self.assertTrue(args.list_recipes)

    def test_recipe_filter(self):
        """--recipe-filter should accept a string."""
        args = self.parser.parse_args(["--recipe-filter", "llvm,lua"])
        self.assertEqual(args.recipe_filter, "llvm,lua")

    def test_cache_options(self):
        """Cache options should be accepted."""
        args = self.parser.parse_args(["--cache-dir", "/cache", "--cache-key", "llvm", "--os-key", "ubuntu:24.04"])
        self.assertEqual(args.cache_dir, "/cache")
        self.assertEqual(args.cache_key, "llvm")
        self.assertEqual(args.os_key, "ubuntu:24.04")

    def test_env_file(self):
        """--env-file should be accepted."""
        args = self.parser.parse_args(["--env-file", "/tmp/env"])
        self.assertEqual(args.env_file, "/tmp/env")

    def test_run_config_options(self):
        """--generate-run-configs and --no-run-configs should toggle the same dest."""
        args1 = self.parser.parse_args(["--generate-run-configs"])
        self.assertTrue(args1.generate_run_configs)
        args2 = self.parser.parse_args(["--no-run-configs"])
        self.assertFalse(args2.generate_run_configs)

    def test_build_tests_toggle(self):
        """--build-tests and --no-build-tests should toggle."""
        args1 = self.parser.parse_args(["--build-tests"])
        self.assertTrue(args1.build_tests)
        args2 = self.parser.parse_args(["--no-build-tests"])
        self.assertFalse(args2.build_tests)

    def test_sanitizer_choices(self):
        """--sanitizer should accept valid sanitizers."""
        args = self.parser.parse_args(["--sanitizer", "address"])
        self.assertEqual(args.sanitizer, "address")

    def test_cflags_cxxflags_ldflags(self):
        """Flag options should be accepted."""
        args = self.parser.parse_args(["--cflags=-O2", "--cxxflags=-std=c++20", "--ldflags=-lm"])
        self.assertEqual(args.cflags, "-O2")
        self.assertEqual(args.cxxflags, "-std=c++20")
        self.assertEqual(args.ldflags, "-lm")

    def test_install_system_deps(self):
        """--install-system-deps should be accepted."""
        args = self.parser.parse_args(["--install-system-deps"])
        self.assertTrue(args.install_system_deps)

    def test_refresh_all(self):
        """--refresh-all should be accepted."""
        args = self.parser.parse_args(["--refresh-all"])
        self.assertTrue(args.refresh_all)

    def test_defaults_are_none(self):
        """Most defaults should be None (omitted from result dict)."""
        args = self.parser.parse_args([])
        self.assertIsNone(args.build_type)
        self.assertIsNone(args.preset)
        self.assertIsNone(args.cc)
        self.assertIsNone(args.cxx)
        self.assertIsNone(args.source_dir)


# ────────────────────────────────────────────────────────────────────
# get_command_line_args
# ────────────────────────────────────────────────────────────────────

class TestGetCommandLineArgs(unittest.TestCase):
    """Test get_command_line_args parsing."""

    def test_empty_args_omits_none_values(self):
        """No args should omit None values but include False booleans from store_true."""
        result = get_command_line_args([])
        # store_true defaults to False (not None), so they are included
        self.assertNotIn("build_type", result)
        self.assertNotIn("preset", result)
        self.assertNotIn("cc", result)
        # But store_true flags default to False, which is not None
        self.assertIn("non_interactive", result)
        self.assertFalse(result["non_interactive"])

    def test_non_none_values_included(self):
        """Non-None values should appear in the dict."""
        result = get_command_line_args(["--build-type", "Debug", "--yes"])
        self.assertEqual(result["build_type"], "Debug")
        self.assertTrue(result["non_interactive"])

    def test_hyphens_converted_to_underscores(self):
        """Argument names with hyphens should be converted to underscores."""
        result = get_command_line_args(["--source-dir", "/src"])
        self.assertIn("source_dir", result)
        self.assertNotIn("source-dir", result)

    def test_false_booleans_included(self):
        """False booleans (--no-build-tests) should be included since value is False, not None."""
        result = get_command_line_args(["--no-build-tests"])
        self.assertIn("build_tests", result)
        self.assertFalse(result["build_tests"])


# ────────────────────────────────────────────────────────────────────
# main()
# ────────────────────────────────────────────────────────────────────

class TestMain(unittest.TestCase):
    """Test main() entry point with mocked MrDocsInstaller."""

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_cache_key_prints_and_exits(self, mock_get_args, mock_installer_cls):
        """--cache-key should print key and return 0."""
        mock_get_args.return_value = {"cache_key": "llvm", "non_interactive": True, "plain_ui": True}
        mock_inst = MagicMock()
        mock_inst.get_cache_key.return_value = "abc123"
        mock_installer_cls.return_value = mock_inst

        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            ret = main()
        self.assertEqual(ret, 0)
        self.assertIn("abc123", mock_out.getvalue())
        mock_inst.get_cache_key.assert_called_once_with("llvm")

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_list_recipes_and_exits(self, mock_get_args, mock_installer_cls):
        """--list-recipes should call list_recipes and return 0."""
        mock_get_args.return_value = {"list_recipes": True, "non_interactive": True, "plain_ui": True}
        mock_inst = MagicMock()
        mock_installer_cls.return_value = mock_inst

        ret = main()
        self.assertEqual(ret, 0)
        mock_inst.list_recipes.assert_called_once()

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_refresh_all_and_exits(self, mock_get_args, mock_installer_cls):
        """--refresh-all should call refresh_all and return 0."""
        mock_get_args.return_value = {"refresh_all": True, "non_interactive": True, "plain_ui": True}
        mock_inst = MagicMock()
        mock_installer_cls.return_value = mock_inst

        ret = main()
        self.assertEqual(ret, 0)
        mock_inst.refresh_all.assert_called_once()

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_normal_run(self, mock_get_args, mock_installer_cls):
        """Normal invocation should call installer.run() and return 0."""
        mock_get_args.return_value = {"non_interactive": True, "plain_ui": True}
        mock_inst = MagicMock()
        mock_installer_cls.return_value = mock_inst

        ret = main()
        self.assertEqual(ret, 0)
        mock_inst.run.assert_called_once()

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_keyboard_interrupt(self, mock_get_args, mock_installer_cls):
        """KeyboardInterrupt should return 130."""
        mock_get_args.side_effect = KeyboardInterrupt()

        with patch("sys.stdout", new_callable=io.StringIO):
            ret = main()
        self.assertEqual(ret, 130)

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_exception_returns_1(self, mock_get_args, mock_installer_cls):
        """Generic exception should print error and return 1."""
        mock_get_args.return_value = {"non_interactive": True, "plain_ui": True}
        mock_installer_cls.side_effect = RuntimeError("something went wrong")

        with patch("sys.stderr", new_callable=io.StringIO) as mock_err:
            ret = main()
        self.assertEqual(ret, 1)
        self.assertIn("something went wrong", mock_err.getvalue())

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_exception_debug_reraises(self, mock_get_args, mock_installer_cls):
        """In debug mode, exceptions should be re-raised."""
        mock_get_args.return_value = {"debug": True, "non_interactive": True, "plain_ui": True}
        mock_installer_cls.side_effect = RuntimeError("debug error")

        with self.assertRaises(RuntimeError):
            main()

    @patch("src.__main__.MrDocsInstaller")
    @patch("src.__main__.get_command_line_args")
    def test_cache_key_takes_priority_over_list_recipes(self, mock_get_args, mock_installer_cls):
        """cache_key check runs before list_recipes."""
        mock_get_args.return_value = {
            "cache_key": "lua", "list_recipes": True,
            "non_interactive": True, "plain_ui": True
        }
        mock_inst = MagicMock()
        mock_inst.get_cache_key.return_value = "key123"
        mock_installer_cls.return_value = mock_inst

        with patch("sys.stdout", new_callable=io.StringIO):
            ret = main()
        self.assertEqual(ret, 0)
        mock_inst.get_cache_key.assert_called_once()
        mock_inst.list_recipes.assert_not_called()


# ────────────────────────────────────────────────────────────────────
# TextUI — uncovered paths
# ────────────────────────────────────────────────────────────────────

class TestTextUISupportChecks(unittest.TestCase):
    """Test _supports_color and _supports_emoji static methods."""

    @patch.dict(os.environ, {"NO_COLOR": "1"}, clear=False)
    def test_supports_color_no_color_env(self):
        """NO_COLOR should disable color."""
        self.assertFalse(TextUI._supports_color())

    @patch.dict(os.environ, {"BOOTSTRAP_PLAIN": "1"}, clear=False)
    def test_supports_color_bootstrap_plain(self):
        """BOOTSTRAP_PLAIN should disable color."""
        self.assertFalse(TextUI._supports_color())

    @patch.dict(os.environ, {"BOOTSTRAP_PLAIN": "1"}, clear=False)
    def test_supports_emoji_bootstrap_plain(self):
        """BOOTSTRAP_PLAIN should disable emoji."""
        self.assertFalse(TextUI._supports_emoji())

    @patch.dict(os.environ, {}, clear=False)
    def test_supports_emoji_default(self):
        """Without BOOTSTRAP_PLAIN, emoji should be supported."""
        env = os.environ.copy()
        env.pop("BOOTSTRAP_PLAIN", None)
        with patch.dict(os.environ, env, clear=True):
            self.assertTrue(TextUI._supports_emoji())


class TestTextUIFmtColor(unittest.TestCase):
    """Test _fmt with color enabled."""

    def setUp(self):
        self.ui = TextUI.__new__(TextUI)
        self.ui.color_enabled = True
        self.ui.emoji_enabled = False
        self.ui.max_path = 50
        self.ui.base_path = None
        self.ui.base_token = "."
        self.ui.dry_run = False

    def test_fmt_with_color(self):
        """_fmt with color should wrap text in ANSI codes."""
        result = self.ui._fmt("hello", "info")
        self.assertIn("\033[", result)
        self.assertIn("hello", result)
        self.assertIn(TextUI.COLOR["reset"], result)

    def test_fmt_unknown_kind(self):
        """Unknown kind should use empty color string."""
        result = self.ui._fmt("msg", "nonexistent")
        self.assertIn("msg", result)


class TestTextUIFmtEmoji(unittest.TestCase):
    """Test _fmt with emoji enabled."""

    def setUp(self):
        self.ui = TextUI.__new__(TextUI)
        self.ui.color_enabled = False
        self.ui.emoji_enabled = True
        self.ui.max_path = 50
        self.ui.base_path = None
        self.ui.base_token = "."
        self.ui.dry_run = False

    def test_fmt_with_emoji(self):
        """_fmt with emoji should prepend emoji prefix."""
        result = self.ui._fmt("warning", "warn")
        self.assertIn("\u26a0", result)
        self.assertIn("warning", result)

    def test_fmt_custom_icon(self):
        """_fmt with custom icon should use it instead of default."""
        result = self.ui._fmt("msg", "info", icon="*")
        self.assertTrue(result.startswith("* "))

    def test_fmt_icon_no_trailing_space_gets_one(self):
        """If icon doesn't end with space, one is added."""
        result = self.ui._fmt("msg", "info", icon="X")
        self.assertTrue(result.startswith("X "))

    def test_fmt_icon_with_trailing_space_kept(self):
        """If icon ends with space, no extra space added."""
        result = self.ui._fmt("msg", "info", icon="X ")
        self.assertTrue(result.startswith("X "))
        self.assertFalse(result.startswith("X  "))


class TestTextUISubsectionNonPlain(unittest.TestCase):
    """Test subsection in non-plain mode (with color)."""

    def setUp(self):
        self.ui = TextUI.__new__(TextUI)
        self.ui.color_enabled = True
        self.ui.emoji_enabled = False
        self.ui.max_path = 50
        self.ui.base_path = None
        self.ui.base_token = "."
        self.ui.dry_run = False

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_subsection_non_plain_has_underline(self, mock_out):
        """Non-plain subsection should have an underline."""
        self.ui.subsection("My Section")
        output = mock_out.getvalue()
        self.assertIn("My Section", output)
        self.assertIn("-", output)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_subsection_non_plain_indented(self, mock_out):
        """Non-plain subsection should use indented format (no '---' prefix)."""
        self.ui.subsection("Test")
        output = mock_out.getvalue()
        self.assertNotIn("--- ", output)


class TestTextUIShortenPath(unittest.TestCase):
    """Test shorten_path edge cases."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)
        self.ui.set_base_path("/home/user/mrdocs")

    def test_empty_path(self):
        """Empty string should return empty."""
        self.assertEqual(self.ui.shorten_path(""), "")

    def test_cwd_returns_dot(self):
        """Path matching cwd should return '.'."""
        cwd = os.getcwd()
        self.assertEqual(self.ui.shorten_path(cwd), ".")

    def test_long_path_ellipsis(self):
        """Path longer than max_path with many components should be ellipsized."""
        self.ui.max_path = 20
        self.ui.base_path = None
        long_path = "/a/b/c/d/e/f/g/h/i/j/k/long_filename.txt"
        result = self.ui.shorten_path(long_path)
        self.assertIn("...", result)

    def test_short_path_with_few_parts(self):
        """Path with <= 4 parts should not be ellipsized even if long."""
        self.ui.max_path = 10
        self.ui.base_path = None
        path = "/very_long_directory_name/another_very_long_name/file.txt"
        result = self.ui.shorten_path(path)
        # 3 parts (after leading /), so should not be truncated
        self.assertNotIn("...", result)

    def test_base_token_prefix_skips_abspath(self):
        """Path starting with base_token should not be made absolute."""
        result = self.ui.shorten_path("./relative/file.txt")
        # Should start with . (base_token) or be shortened
        self.assertIsInstance(result, str)

    def test_mrdocs_prefix_skips_abspath(self):
        """Path starting with $MRDOCS should not be made absolute."""
        result = self.ui.shorten_path("$MRDOCS/build/file.txt")
        self.assertTrue(result.startswith("$MRDOCS"))

    @patch("src.core.ui.os.getcwd", side_effect=OSError("no cwd"))
    def test_shorten_path_getcwd_exception(self, _mock):
        """shorten_path should handle getcwd exception gracefully."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        # Should not raise, just skip the cwd comparison
        result = ui.shorten_path("/some/path")
        self.assertIsInstance(result, str)


class TestTextUIShortenMiddle(unittest.TestCase):
    """Test _shorten_middle static method."""

    def test_short_text_unchanged(self):
        """Text shorter than max_len should be unchanged."""
        self.assertEqual(TextUI._shorten_middle("hello", 10), "hello")

    def test_long_text_ellipsized(self):
        """Text longer than max_len should be truncated with '...'."""
        result = TextUI._shorten_middle("a" * 100, 20)
        self.assertIn("...", result)
        self.assertLessEqual(len(result), 100)

    def test_exact_length_unchanged(self):
        """Text exactly max_len should be unchanged."""
        text = "exactly_20_chars!..."
        self.assertEqual(TextUI._shorten_middle(text, len(text)), text)


class TestTextUIMaybeShorten(unittest.TestCase):
    """Test maybe_shorten with various input types."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)
        self.ui.set_base_path("/home/user/mrdocs")

    def test_non_string_passthrough(self):
        """Non-string values should be returned unchanged."""
        self.assertEqual(self.ui.maybe_shorten(42), 42)

    def test_url_unchanged(self):
        """HTTP URLs should be returned unchanged."""
        url = "https://github.com/cppalliance/mrdocs"
        self.assertEqual(self.ui.maybe_shorten(url), url)

    def test_http_url_unchanged(self):
        """http:// URLs should also be returned unchanged."""
        url = "http://example.com/path"
        self.assertEqual(self.ui.maybe_shorten(url), url)

    def test_base_token_prefix(self):
        """Value starting with base_token should be shortened via _shorten_middle."""
        self.ui.max_path = 20
        val = "./very/long/path/to/some/deeply/nested/file.txt"
        result = self.ui.maybe_shorten(val)
        self.assertIsInstance(result, str)

    def test_mrdocs_prefix(self):
        """Value starting with $MRDOCS should be shortened via _shorten_middle."""
        val = "$MRDOCS/something"
        result = self.ui.maybe_shorten(val)
        self.assertIn("$MRDOCS", result)

    def test_tilde_prefix(self):
        """Value starting with ~ should be shortened via _shorten_middle."""
        val = "~/projects/mrdocs/build/file.txt"
        result = self.ui.maybe_shorten(val)
        self.assertIsInstance(result, str)

    def test_path_under_base_path_replaced(self):
        """Absolute path under base_path should get base_token replacement."""
        val = "/home/user/mrdocs/build/output/file.txt"
        result = self.ui.maybe_shorten(val)
        self.assertIn(".", result)

    def test_pathish_outside_base(self):
        """Path-like value outside base_path should still be shortened."""
        val = "/other/location/build/file.txt"
        result = self.ui.maybe_shorten(val)
        self.assertIsInstance(result, str)

    def test_non_path_passthrough(self):
        """Non-path non-URL strings should be returned as-is."""
        val = "just_a_word"
        self.assertEqual(self.ui.maybe_shorten(val), val)

    def test_maybe_shorten_abspath_exception(self):
        """maybe_shorten should handle abspath exceptions in pathish branch gracefully."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.base_path = "/home/user/mrdocs"
        ui.base_token = "."
        # Raise only on the first call (inside the pathish try/except in maybe_shorten),
        # then return normally for the fallback shorten_path calls.
        real_abspath = os.path.abspath
        call_count = [0]
        def selective_abspath(p):
            call_count[0] += 1
            if call_count[0] == 1:
                raise OSError("broken")
            return real_abspath(p)
        with patch("src.core.ui.os.path.abspath", side_effect=selective_abspath):
            result = ui.maybe_shorten("/some/other/path/file.txt")
        self.assertIsInstance(result, str)


class TestTextUIKv(unittest.TestCase):
    """Test kv() key-value output."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_kv_prints_key_value(self, mock_out):
        """kv() should print right-aligned key and value."""
        self.ui.kv("Name", "MrDocs")
        output = mock_out.getvalue()
        self.assertIn("Name", output)
        self.assertIn("MrDocs", output)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_kv_shortens_paths(self, mock_out):
        """kv() should shorten path-like values."""
        self.ui.set_base_path("/home/user/mrdocs")
        self.ui.kv("Path", "/home/user/mrdocs/build/out")
        output = mock_out.getvalue()
        self.assertIn("Path", output)


class TestTextUIKvBlock(unittest.TestCase):
    """Test kv_block() method."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_kv_block_with_title(self, mock_out):
        """kv_block with title should print section header and items."""
        self.ui.kv_block("Config", [("key1", "val1"), ("key2", "val2")])
        output = mock_out.getvalue()
        self.assertIn("Config", output)
        self.assertIn("key1", output)
        self.assertIn("val1", output)
        self.assertIn("key2", output)
        self.assertIn("val2", output)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_kv_block_no_title(self, mock_out):
        """kv_block without title should skip section header."""
        self.ui.kv_block(None, [("k", "v")])
        output = mock_out.getvalue()
        self.assertIn("k", output)
        self.assertIn("v", output)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_kv_block_empty_items(self, mock_out):
        """kv_block with empty items should print nothing (or just title)."""
        self.ui.kv_block("Header", [])
        output = mock_out.getvalue()
        # Title section is printed, but no items
        self.assertIn("Header", output)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_kv_block_with_color(self, mock_out):
        """kv_block with color enabled should use ANSI codes."""
        ui = TextUI.__new__(TextUI)
        ui.color_enabled = True
        ui.emoji_enabled = False
        ui.max_path = 50
        ui.base_path = None
        ui.base_token = "."
        ui.dry_run = False
        ui.kv_block(None, [("key", "value")])
        output = mock_out.getvalue()
        self.assertIn("\033[", output)


class TestTextUIChecklistNonPlain(unittest.TestCase):
    """Test checklist() in non-plain (Unicode) mode."""

    def setUp(self):
        self.ui = TextUI.__new__(TextUI)
        self.ui.color_enabled = True
        self.ui.emoji_enabled = False
        self.ui.max_path = 50
        self.ui.base_path = None
        self.ui.base_token = "."
        self.ui.dry_run = False

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_checklist_unicode_marks(self, mock_out):
        """Non-plain checklist should use Unicode check/cross marks."""
        self.ui.checklist("", [("done", True), ("todo", False)])
        output = mock_out.getvalue()
        self.assertIn("\u2713", output)  # checkmark
        self.assertIn("\u2717", output)  # cross

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_checklist_with_title(self, mock_out):
        """Checklist with title should print section header."""
        self.ui.checklist("Status", [("item", True)])
        output = mock_out.getvalue()
        self.assertIn("Status", output)
        self.assertIn("item", output)


class TestTextUIStep(unittest.TestCase):
    """Test step() method."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_step_prints_progress(self, mock_out):
        """step() should print [current/total] prefix."""
        self.ui.step(1, 5, "Building")
        output = mock_out.getvalue()
        self.assertIn("[1/5]", output)
        self.assertIn("Building", output)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_step_last(self, mock_out):
        """step() should work for last step."""
        self.ui.step(5, 5, "Done")
        output = mock_out.getvalue()
        self.assertIn("[5/5]", output)
        self.assertIn("Done", output)


class TestTextUICommand(unittest.TestCase):
    """Test command() method."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    @patch("sys.stdout", new_callable=io.StringIO)
    def test_command_prints(self, mock_out):
        """command() should print the command string."""
        self.ui.command("cmake --build .")
        self.assertIn("cmake --build .", mock_out.getvalue())


class TestTextUIDryRunOutput(unittest.TestCase):
    """Test _out property in dry-run mode."""

    def test_dry_run_info_goes_to_stderr(self):
        """In dry-run mode, _out should return stderr."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = True
        self.assertIs(ui._out, sys.stderr)

    def test_normal_info_goes_to_stdout(self):
        """In normal mode, _out should return stdout."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = False
        self.assertIs(ui._out, sys.stdout)


class TestTextUIErrorBlock(unittest.TestCase):
    """Test error_block with and without tips."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    @patch("sys.stderr", new_callable=io.StringIO)
    def test_error_block_no_tips(self, mock_err):
        """error_block without tips should print only header."""
        self.ui.error_block("Something failed")
        output = mock_err.getvalue()
        self.assertIn("Something failed", output)

    @patch("sys.stderr", new_callable=io.StringIO)
    def test_error_block_multiple_tips(self, mock_err):
        """error_block with multiple tips should print all."""
        self.ui.error_block("Failed", ["tip1", "tip2", "tip3"])
        output = mock_err.getvalue()
        self.assertIn("Failed", output)
        self.assertIn("tip1", output)
        self.assertIn("tip2", output)
        self.assertIn("tip3", output)


if __name__ == "__main__":
    unittest.main()
