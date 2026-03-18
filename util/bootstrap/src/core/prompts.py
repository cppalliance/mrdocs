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

"""
User prompting utilities for the bootstrap tool.

Provides functions for prompting users for input during the
bootstrap process.
"""

from typing import Optional, List

from .platform import supports_ansi
from .ui import TextUI, get_default_ui


# ANSI color codes
BLUE = "\033[94m"
RESET = "\033[0m"


def prompt_string(
    prompt: str,
    default: Optional[str] = None,
    non_interactive: bool = False,
    ui: Optional[TextUI] = None,
) -> str:
    """
    Prompt the user for a string input with a default value.

    Args:
        prompt: The prompt message to display.
        default: The default value to use if no input provided.
        non_interactive: If True, return default without prompting.
        ui: TextUI instance for path shortening.

    Returns:
        The user's input or the default value.
    """
    if ui is None:
        ui = get_default_ui()

    indent = "    "
    if non_interactive:
        if default is not None:
            return default
        raise RuntimeError(f"No default for prompt '{prompt}' in non-interactive mode")

    prompt = prompt.strip()
    if prompt.endswith('.'):
        prompt = prompt[:-1].strip()

    display_default = default
    if isinstance(default, str):
        try:
            display_default = ui.maybe_shorten(default)
        except Exception:
            display_default = default

    if supports_ansi() and (ui is None or ui.color_enabled):
        prompt = f"{BLUE}{prompt}{RESET}"
    if display_default not in (None, ""):
        prompt += f" ({display_default})"
    prompt += ": "

    inp = input(indent + prompt)
    return inp.strip() or default


def prompt_boolean(
    prompt: str,
    default: Optional[bool] = None,
    non_interactive: bool = False,
    ui: Optional[TextUI] = None,
) -> bool:
    """
    Prompt the user for a boolean value (yes/no).

    Args:
        prompt: The prompt message to display.
        default: The default value if no input provided.
        non_interactive: If True, return default without prompting.
        ui: TextUI instance for warnings.

    Returns:
        True if the user answers yes, False otherwise.
    """
    if ui is None:
        ui = get_default_ui()

    indent = "    "
    if non_interactive:
        if default is not None:
            return default
        raise RuntimeError(f"No default for prompt '{prompt}' in non-interactive mode")

    prompt = prompt.strip()
    if prompt.endswith('.'):
        prompt = prompt[:-1].strip()

    if supports_ansi() and (ui is None or ui.color_enabled):
        prompt = f"{BLUE}{prompt}{RESET}"

    while True:
        hint = 'y/n' if default is None else ('yes' if default else 'no')
        answer = input(f"{indent}{prompt} ({hint}): ").strip().lower()
        if not answer and default is not None:
            return default
        if answer in ('y', 'yes', '1', 'true'):
            return True
        elif answer in ('n', 'no', '0', 'false'):
            return False
        else:
            ui.warn("Invalid input. Please answer 'yes' or 'no'.")


def prompt_choice(
    prompt: str,
    choices: List[str],
    default: Optional[str] = None,
    non_interactive: bool = False,
    normalizer: Optional[callable] = None,
) -> str:
    """
    Prompt the user to select from a list of choices.

    Args:
        prompt: The prompt message to display.
        choices: List of valid choices.
        default: The default choice.
        non_interactive: If True, return default without prompting.
        normalizer: Optional function to normalize input before matching.

    Returns:
        The selected choice.
    """
    indent = "    "
    if non_interactive:
        if default is not None:
            return default
        raise RuntimeError(f"No default for prompt '{prompt}' in non-interactive mode")

    prompt = prompt.strip()
    if prompt.endswith('.'):
        prompt = prompt[:-1].strip()

    choices_lower = [c.lower() for c in choices]

    if supports_ansi():
        prompt = f"{BLUE}{prompt}{RESET}"

    choices_str = "/".join(choices)
    if default:
        prompt += f" ({choices_str}, default: {default})"
    else:
        prompt += f" ({choices_str})"
    prompt += ": "

    while True:
        answer = input(indent + prompt).strip()
        if not answer and default is not None:
            return default

        normalized = normalizer(answer) if normalizer else answer.lower()
        if normalized in choices_lower:
            idx = choices_lower.index(normalized)
            return choices[idx]

        print(f"Please enter one of: {choices_str}")
