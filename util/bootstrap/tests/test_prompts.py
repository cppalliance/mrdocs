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

"""Tests for core/prompts.py prompt functions."""

import sys
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.core.prompts import prompt_string, prompt_boolean, prompt_choice


class TestPromptStringNonInteractive(unittest.TestCase):
    """Tests for prompt_string non-interactive path."""

    def test_returns_default_without_calling_input(self):
        with patch("builtins.input") as mock_input:
            result = prompt_string("Enter name", default="alice", non_interactive=True)
        self.assertEqual(result, "alice")
        mock_input.assert_not_called()

    def test_raises_when_no_default(self):
        with self.assertRaises(RuntimeError):
            prompt_string("Enter name", non_interactive=True)

    def test_returns_none_default(self):
        """non_interactive with default=None should raise."""
        with self.assertRaises(RuntimeError):
            prompt_string("Enter name", default=None, non_interactive=True)


class TestPromptStringInteractive(unittest.TestCase):
    """Tests for prompt_string interactive path."""

    @patch("builtins.input", return_value="bob")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_returns_user_input(self, _ansi, _inp):
        result = prompt_string("Enter name", default="alice")
        self.assertEqual(result, "bob")

    @patch("builtins.input", return_value="")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_empty_input_returns_default(self, _ansi, _inp):
        result = prompt_string("Enter name", default="alice")
        self.assertEqual(result, "alice")

    @patch("builtins.input", return_value="  spaced  ")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_strips_whitespace(self, _ansi, _inp):
        result = prompt_string("Enter name", default="alice")
        self.assertEqual(result, "spaced")

    @patch("builtins.input", return_value="val")
    @patch("src.core.prompts.supports_ansi", return_value=True)
    def test_ansi_prompt_still_works(self, _ansi, _inp):
        ui = MagicMock()
        ui.color_enabled = True
        ui.maybe_shorten.side_effect = lambda x: x
        result = prompt_string("Enter name.", default="/some/path", ui=ui)
        self.assertEqual(result, "val")

    @patch("builtins.input", return_value="val")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_trailing_dot_stripped_from_prompt(self, _ansi, mock_input):
        prompt_string("Enter name.", default="x")
        # The prompt text passed to input() should not end with a period before the default
        call_arg = mock_input.call_args[0][0]
        self.assertNotIn(".. ", call_arg)

    @patch("builtins.input", return_value="val")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_no_default_shows_no_parens(self, _ansi, mock_input):
        prompt_string("Enter name", default=None)
        call_arg = mock_input.call_args[0][0]
        # Should just end with ": " without a default in parens
        self.assertTrue(call_arg.strip().endswith(":"))

    @patch("builtins.input", return_value="val")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_empty_string_default_shows_no_parens(self, _ansi, mock_input):
        prompt_string("Enter name", default="")
        call_arg = mock_input.call_args[0][0]
        self.assertTrue(call_arg.strip().endswith(":"))


class TestPromptBoolean(unittest.TestCase):
    """Tests for prompt_boolean with yes/no/default inputs."""

    def test_non_interactive_returns_default_true(self):
        result = prompt_boolean("Continue?", default=True, non_interactive=True)
        self.assertTrue(result)

    def test_non_interactive_returns_default_false(self):
        result = prompt_boolean("Continue?", default=False, non_interactive=True)
        self.assertFalse(result)

    def test_non_interactive_no_default_raises(self):
        with self.assertRaises(RuntimeError):
            prompt_boolean("Continue?", non_interactive=True)

    @patch("builtins.input", return_value="y")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_yes_answers(self, _ansi, _inp):
        self.assertTrue(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="yes")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_yes_word(self, _ansi, _inp):
        self.assertTrue(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="1")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_one_is_true(self, _ansi, _inp):
        self.assertTrue(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="true")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_true_word(self, _ansi, _inp):
        self.assertTrue(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="n")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_no_answer(self, _ansi, _inp):
        self.assertFalse(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="no")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_no_word(self, _ansi, _inp):
        self.assertFalse(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="0")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_zero_is_false(self, _ansi, _inp):
        self.assertFalse(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="false")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_false_word(self, _ansi, _inp):
        self.assertFalse(prompt_boolean("Continue?"))

    @patch("builtins.input", return_value="")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_empty_returns_default_true(self, _ansi, _inp):
        self.assertTrue(prompt_boolean("Continue?", default=True))

    @patch("builtins.input", return_value="")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_empty_returns_default_false(self, _ansi, _inp):
        self.assertFalse(prompt_boolean("Continue?", default=False))

    @patch("builtins.input", side_effect=["maybe", "y"])
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_invalid_then_valid(self, _ansi, _inp):
        ui = MagicMock()
        ui.color_enabled = False
        result = prompt_boolean("Continue?", ui=ui)
        self.assertTrue(result)
        ui.warn.assert_called_once()

    @patch("builtins.input", return_value="y")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_hint_shows_yn_when_no_default(self, _ansi, mock_input):
        prompt_boolean("Continue?")
        call_arg = mock_input.call_args[0][0]
        self.assertIn("y/n", call_arg)

    @patch("builtins.input", return_value="y")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_hint_shows_yes_when_default_true(self, _ansi, mock_input):
        prompt_boolean("Continue?", default=True)
        call_arg = mock_input.call_args[0][0]
        self.assertIn("yes", call_arg)

    @patch("builtins.input", return_value="n")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_hint_shows_no_when_default_false(self, _ansi, mock_input):
        prompt_boolean("Continue?", default=False)
        call_arg = mock_input.call_args[0][0]
        self.assertIn("no", call_arg)


class TestPromptChoice(unittest.TestCase):
    """Tests for prompt_choice with valid selection, invalid retry, and non-interactive."""

    def test_non_interactive_returns_default(self):
        result = prompt_choice("Pick", ["a", "b"], default="b", non_interactive=True)
        self.assertEqual(result, "b")

    def test_non_interactive_no_default_raises(self):
        with self.assertRaises(RuntimeError):
            prompt_choice("Pick", ["a", "b"], non_interactive=True)

    @patch("builtins.input", return_value="alpha")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_valid_selection(self, _ansi, _inp):
        result = prompt_choice("Pick", ["Alpha", "Beta"])
        self.assertEqual(result, "Alpha")

    @patch("builtins.input", return_value="beta")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_case_insensitive(self, _ansi, _inp):
        result = prompt_choice("Pick", ["Alpha", "Beta"])
        self.assertEqual(result, "Beta")

    @patch("builtins.input", return_value="")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_empty_returns_default(self, _ansi, _inp):
        result = prompt_choice("Pick", ["Alpha", "Beta"], default="Beta")
        self.assertEqual(result, "Beta")

    @patch("builtins.input", side_effect=["invalid", "alpha"])
    @patch("src.core.prompts.supports_ansi", return_value=False)
    @patch("builtins.print")
    def test_invalid_then_valid(self, mock_print, _ansi, _inp):
        result = prompt_choice("Pick", ["Alpha", "Beta"])
        self.assertEqual(result, "Alpha")
        mock_print.assert_called_once()

    @patch("builtins.input", return_value="custom")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_normalizer(self, _ansi, _inp):
        result = prompt_choice(
            "Pick", ["Alpha", "Beta"],
            normalizer=lambda x: "alpha"  # always normalize to alpha
        )
        self.assertEqual(result, "Alpha")

    @patch("builtins.input", return_value="alpha")
    @patch("src.core.prompts.supports_ansi", return_value=True)
    def test_ansi_prompt(self, _ansi, _inp):
        result = prompt_choice("Pick.", ["Alpha", "Beta"])
        self.assertEqual(result, "Alpha")

    @patch("builtins.input", return_value="alpha")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_choices_shown_in_prompt(self, _ansi, mock_input):
        prompt_choice("Pick", ["Alpha", "Beta"], default="Alpha")
        call_arg = mock_input.call_args[0][0]
        self.assertIn("Alpha/Beta", call_arg)
        self.assertIn("default: Alpha", call_arg)

    @patch("builtins.input", return_value="alpha")
    @patch("src.core.prompts.supports_ansi", return_value=False)
    def test_no_default_no_default_text(self, _ansi, mock_input):
        prompt_choice("Pick", ["Alpha", "Beta"])
        call_arg = mock_input.call_args[0][0]
        self.assertIn("Alpha/Beta", call_arg)
        self.assertNotIn("default:", call_arg)


if __name__ == "__main__":
    unittest.main()
