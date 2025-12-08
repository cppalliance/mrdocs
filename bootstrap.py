#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Heads up (Dec 2025): bootstrap.py is still moving toward being the single
# setup path for ci.yml. Some presets/paths (e.g., release-msvc vs. old
# release-windows) and edge flags may be untested. Defaults can shift while we
# finish the move. If it blows up: 1) wipe the build dir; 2) run the matching
# CMake/Ninja preset by hand; 3) share the failing command. This note stays
# until Bootstrap owns the CI flow.

TRANSITION_BANNER = (
    "Heads up: bootstrap.py is mid-move to replace the process in ci.yml; presets can differ. "
    "If it fails, try a clean build dir or run the preset yourself."
)

import argparse
import subprocess
import os
import sys
import platform
import shutil
import math
from dataclasses import dataclass, field
import dataclasses
import urllib.request
import tarfile
import json
import shlex
import re
import zipfile
from pathlib import Path
from functools import lru_cache
from typing import Optional, Dict, Any, List, Iterable, Set


@lru_cache(maxsize=1)
def running_from_mrdocs_source_dir():
    """
    Checks if the current working directory is the same as the script directory.
    :return:
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    cwd = os.getcwd()
    return cwd == script_dir


class TextUI:
    """
    Lightweight console formatting helper that keeps default output plain
    but can emit color/emoji when available or explicitly enabled.
    """

    COLOR = {
        "reset": "\033[0m",
        "info": "\033[97m",          # bright white
        "warn": "\033[93m",          # yellow
        "error": "\033[1;91m",       # bold red
        "ok": "\033[1;92m",          # bold green
        "section": "\033[1;96m",     # bold cyan
        "subsection": "\033[96m",    # cyan
        "command": "\033[95m",       # magenta
        "dim": "\033[2m",
    }
    EMOJI = {
        "info": "",
        "warn": "⚠️ ",
        "error": "⛔ ",
        "ok": "✅ ",
        "section": "",
        "command": "💻 ",
    }

    def __init__(self, enable_color: bool = False, enable_emoji: bool = False):
        force_color = os.environ.get("BOOTSTRAP_FORCE_COLOR") or os.environ.get("CLICOLOR_FORCE")
        force_emoji = os.environ.get("BOOTSTRAP_FORCE_EMOJI")
        self.color_enabled = bool(enable_color and (force_color or self._supports_color()))
        self.emoji_enabled = bool(enable_emoji and (force_emoji or self._supports_emoji()))
        # keep displayed paths compact; we shorten aggressively past this length
        self.max_path = 50
        self.base_path: Optional[str] = None
        self.base_token: str = "."

    @staticmethod
    def _supports_color() -> bool:
        if os.environ.get("NO_COLOR") or os.environ.get("BOOTSTRAP_PLAIN"):
            return False
        return sys.stdout.isatty()

    @staticmethod
    def _supports_emoji() -> bool:
        if os.environ.get("BOOTSTRAP_PLAIN"):
            return False
        return True

    def _fmt(self, text: str, kind: str, icon: Optional[str] = None) -> str:
        prefix = ""
        if self.emoji_enabled:
            prefix = icon if icon is not None else self.EMOJI.get(kind, "")
        if not self.color_enabled:
            return f"{prefix}{text}"
        color = self.COLOR.get(kind, "")
        reset = self.COLOR["reset"]
        return f"{color}{prefix}{text}{reset}"

    def info(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "info", icon))

    def warn(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "warn", icon))

    def error(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "error", icon))

    def error_block(self, header: str, tips: Optional[List[str]] = None):
        print(self._fmt(f"!! {header}", "error"))
        if tips:
            for tip in tips:
                print(self._fmt(f"   • {tip}", "warn"))

    def ok(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "ok", icon))

    def section(self, title: str, icon: Optional[str] = None):
        prefix = (icon + " ") if (self.emoji_enabled and icon) else ""
        line = "━" * 60
        print()
        print(self._fmt(line, "section", ""))
        print(self._fmt(f"{prefix}{title}", "section", ""))
        print(self._fmt(line, "section", ""))

    def command(self, cmd: str, icon: Optional[str] = None):
        print(self._fmt(cmd, "command", icon))

    def subsection(self, title: str, icon: Optional[str] = None):
        prefix = (icon + " ") if (self.emoji_enabled and icon) else ""
        banner = f"  {prefix}{title}"
        print()  # blank line for breathing room
        print(self._fmt(banner, "subsection", ""))
        # underline matches text length (indent + title) plus a small cushion
        underline_len = max(15, len(banner.strip()) + 4)
        print(self._fmt("-" * underline_len, "subsection", ""))

    def shorten_path(self, path: str) -> str:
        if not path:
            return path
        try:
            if os.path.abspath(path) == os.path.abspath(os.getcwd()):
                return "."
        except Exception:
            pass
        if not (path.startswith(self.base_token) or path.startswith("$MRDOCS")):
            path = os.path.abspath(path)
        if self.base_path and path.startswith(self.base_path):
            suffix = path[len(self.base_path):]
            if suffix.startswith(os.sep):
                suffix = suffix[1:]
            path = f"{self.base_token}" + (f"/{suffix}" if suffix else "")
        home = os.path.expanduser("~")
        if path.startswith(home):
            path = path.replace(home, "~", 1)
        if len(path) <= self.max_path:
            return path
        parts = path.split(os.sep)
        if len(parts) <= 4:
            return path
        return os.sep.join(parts[:2]) + os.sep + "..." + os.sep + os.sep.join(parts[-2:])

    @staticmethod
    def _shorten_middle(text: str, max_len: int) -> str:
        if len(text) <= max_len:
            return text
        take = max_len // 2 - 2
        return text[:take] + "..." + text[-take:]

    def set_base_path(self, path: Optional[str], token: str = "."):
        if path:
            self.base_path = os.path.abspath(path)
        self.base_token = token

    def maybe_shorten(self, value: str) -> str:
        """
        Shorten likely-path values but leave URLs and simple tokens intact.
        """
        if not isinstance(value, str):
            return value
        lowered = value.lower()
        if lowered.startswith("http://") or lowered.startswith("https://"):
            return value
        if value.startswith(self.base_token) or value.startswith("$MRDOCS") or value.startswith("~"):
            return self._shorten_middle(value, self.max_path)
        is_pathish = (os.sep in value) or value.startswith("~") or value.startswith(".") or value.startswith("/")
        # Prefer replacing the MrDocs source prefix with a short token for path-like strings
        if is_pathish:
            try:
                if self.base_path:
                    abs_val = value if value.startswith(self.base_token) or value.startswith("$MRDOCS") else os.path.abspath(value)
                    if abs_val.startswith(self.base_path):
                        rel = abs_val[len(self.base_path):]
                        if rel.startswith(os.sep):
                            rel = rel[1:]
                        replaced = self.base_token + (f"/{rel}" if rel else "")
                        return self._shorten_middle(replaced, self.max_path)
            except Exception:
                pass
        if is_pathish:
            return self.shorten_path(value)
        return value

    def kv(self, key: str, value: str, key_width: int = 18):
        key_fmt = key.rjust(key_width)
        display_value = self.maybe_shorten(value) if isinstance(value, str) else value
        print(self._fmt(f"{key_fmt}: ", "dim") + self._fmt(display_value, "info"))

    def kv_block(self, title: Optional[str], items: List[tuple], icon: Optional[str] = None, indent: int = 2):
        """
        Print an aligned key-value block with optional header.
        """
        if title:
            self.section(title, icon=icon)
        if not items:
            return
        key_width = max(len(k) for k, _ in items) + 2
        pad = " " * indent
        for k, v in items:
            key_fmt = k.rjust(key_width)
            display_value = self.maybe_shorten(v) if isinstance(v, str) else v
            line = f"{pad}{key_fmt}: "
            if self.color_enabled:
                line = f"{self.COLOR['dim']}{line}{self.COLOR['reset']}"
            print(line + self._fmt(str(display_value), "info"))

    def checklist(self, title: str, items):
        if title:
            self.section(title)
        for label, done in items:
            mark = "✓" if done else "✗"
            style = "ok" if done else "warn"
            print(self._fmt(f"  {mark} {label}", style))

    def step(self, current: int, total: int, title: str):
        prefix = f"[{current}/{total}] "
        print(self._fmt(f"{prefix}{title}", "subsection"))


# default UI; may be replaced once options are parsed
ui = TextUI()


@dataclass
class RecipeSource:
    type: str
    url: str
    branch: Optional[str] = None
    tag: Optional[str] = None
    commit: Optional[str] = None
    ref: Optional[str] = None
    depth: Optional[int] = None
    submodules: bool = False


@dataclass
class Recipe:
    name: str
    version: str
    source: RecipeSource
    dependencies: List[str]
    source_dir: str
    build_dir: str
    install_dir: str
    build_type: str
    source_subdir: Optional[str] = None
    build: List[Dict[str, Any]] = field(default_factory=list)
    tags: List[str] = field(default_factory=list)
    package_root_var: Optional[str] = None
    install_scope: str = "per-preset"  # "per-preset" (default) or "global"


@dataclass
class InstallOptions:
    """
    Stores configuration options for the MrDocs bootstrap installer.

    Note:
        The @dataclass decorator is used to automatically generate special methods for
        the class, such as __init__, __repr__, and __eq__, based on the class attributes.
        This simplifies the creation of classes that are primarily used to store data,
        reducing boilerplate code and improving readability.
        In InstallOptions, it allows easy initialization and management of
        configuration options with default values and type hints.
    """
    # Compiler
    cc: str = ''
    cxx: str = ''
    sanitizer: str = ''

    # Required tools
    git_path: str = ''
    cmake_path: str = ''
    python_path: str = ''

    # Test tools
    java_path: str = ''

    # Optional tools
    ninja_path: str = ''

    # MrDocs
    source_dir: str = field(default_factory=lambda: os.path.dirname(os.path.abspath(__file__)))
    build_type: str = "Release"
    preset: str = "<build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    build_dir: str = "<source-dir>/build/<build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower><\"-\":if(sanitizer)><sanitizer:lower>"
    build_tests: bool = True
    system_install: bool = field(default_factory=lambda: not running_from_mrdocs_source_dir())
    install_dir: str = field(
        default_factory=lambda: "<source-dir>/install/<build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>" if running_from_mrdocs_source_dir() else "")
    run_tests: bool = False

    # Third-party dependencies root and recipes
    third_party_src_dir: str = "<source-dir>/build/third-party"

    # Information to create run configurations
    generate_run_configs: bool = field(default_factory=lambda: running_from_mrdocs_source_dir())
    jetbrains_run_config_dir: str = "<source-dir>/.run"
    boost_src_dir: str = "<source-dir>/../boost"
    generate_clion_run_configs: bool = True
    generate_vscode_run_configs: bool = field(default_factory=lambda: os.name != "nt")
    generate_vs_run_configs: bool = field(default_factory=lambda: os.name == "nt")

    # Information to create pretty printer configs
    generate_pretty_printer_configs: bool = field(default_factory=lambda: running_from_mrdocs_source_dir())

    # Command line arguments
    non_interactive: bool = False
    refresh_all: bool = False
    force_rebuild: bool = False
    remove_build_dir: bool = True
    plain_ui: bool = False
    verbose: bool = False
    debug: bool = False
    dry_run: bool = False
    list_recipes: bool = False
    recipe_filter: str = ""
    skip_build: bool = False
    clean: bool = False
    force: bool = False


class MrDocsInstaller:
    """
    Handles the installation workflow for MrDocs and its third-party dependencies.
    """

    def __init__(self, cmd_line_args=None):
        """
        Initializes the installer with the given options.
        """
        self.cmd_line_args = cmd_line_args or dict()
        self.default_options = InstallOptions()
        self.options = InstallOptions()
        self.package_roots: Dict[str, str] = {}
        self.recipe_info: Dict[str, Recipe] = {}
        for field in dataclasses.fields(self.options):
            if field.type == str:
                setattr(self.options, field.name, '')
            elif field.type == bool:
                setattr(self.options, field.name, None)
            else:
                raise TypeError(f"Unsupported type {field.type} for field {field.name} in InstallOptions.")
        # Seed critical defaults for path expansion before prompting
        self.options.source_dir = self.default_options.source_dir
        self.options.third_party_src_dir = self.default_options.third_party_src_dir
        self.recipes_dir = os.path.join(self.options.source_dir, "third-party", "recipes")
        self.patches_dir = os.path.join(self.options.source_dir, "third-party", "patches")
        self.options.non_interactive = self.cmd_line_args.get("non_interactive", False)
        self.options.refresh_all = self.cmd_line_args.get("refresh_all", False)
        self.prompted_options = set()
        self.compiler_info = {}
        self.env = os.environ.copy()
        # Disable pkg-config to avoid CMake regex issues with paths containing '+' (e.g., C++).
        # Find modules will still use CMAKE_PREFIX_PATH and hints.
        self.env["PKG_CONFIG"] = "false"
        # Seed options from command-line for all fields we already know
        for field in dataclasses.fields(self.options):
            name = field.name
            if name in self.cmd_line_args and self.cmd_line_args[name] is not None:
                setattr(self.options, name, self.cmd_line_args[name])
        plain_ui_flag = bool(self.cmd_line_args.get("plain_ui", False))
        self.ui = TextUI(enable_color=not plain_ui_flag, enable_emoji=not plain_ui_flag)
        # allow UI shortening to replace MrDocs source dir with a compact token
        self.ui.set_base_path(self.options.source_dir)
        global ui
        ui = self.ui
        self.package_roots: Dict[str, str] = {}
        self.recipe_info: Dict[str, Recipe] = {}

    def _load_json_file(self, path: str) -> Optional[Dict[str, Any]]:
        if not os.path.exists(path):
            return None
        try:
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as exc:
            self.ui.warn(f"Failed to read {path}: {exc}")
            return None

    def prompt_string(self, prompt, default):
        """
        Prompts the user for a string input with a default value.

        :param prompt: The prompt message to display to the user.
        :param default: The default value to use if the user does not provide input.
        :return:
        """
        indent = "    "
        if self.options.non_interactive and default is not None:
            return default
        prompt = prompt.strip()
        if prompt.endswith('.'):
            prompt = prompt[:-1]
            prompt = prompt.strip()
        BLUE = "\033[94m"
        RESET = "\033[0m"
        display_default = default
        if isinstance(default, str):
            try:
                display_default = self.ui.maybe_shorten(default)
            except Exception:
                display_default = default

        if self.supports_ansi():
            prompt = f"{BLUE}{prompt}{RESET}"
        if display_default not in (None, ""):
            prompt += f" ({display_default})"
        prompt += f": "
        inp = input(indent + prompt)
        result = inp.strip() or default
        return result

    def prompt_boolean(self, prompt, default=None):
        """
        Prompts the user for a boolean value (yes/no).

        :param prompt: The prompt message to display.
        :param default: The default value to return if the user does not provide input.
        :return: bool: True if the user answers yes, False otherwise.
        """
        indent = "    "
        if self.options.non_interactive and default is not None:
            return default
        prompt = prompt.strip()
        if prompt.endswith('.'):
            prompt = prompt[:-1]
            prompt = prompt.strip()
        BLUE = "\033[94m"
        RESET = "\033[0m"
        if self.supports_ansi():
            prompt = f"{BLUE}{prompt}{RESET}"
        while True:
            answer = input(f"{indent}{prompt} ({'y/n' if default is None else 'yes' if default else 'no'}): ").strip().lower()
            if answer in ('y', 'yes'):
                return True
            elif answer in ('n', 'no'):
                return False
            else:
                if default is not None:
                    return default
                else:
                    print("Please answer 'y or 'n'.")

    def prompt_option(self, name, prompt_text, force_prompt=False):
        """
        Prompts the user for a configuration option based on its name.

        This function will prompt the user for a specific option if
        it has not been prompted before.

        Values come from command line arguments, from the default options,
        or from user input.

        :param name: The name of the option to prompt for.
        :param prompt_text: The text to display when prompting.
        :return: The value of the option after prompting the user.
        """
        name = name.replace("-", "_")

        # If already prompted for this one
        if name in self.prompted_options and not force_prompt:
            return getattr(self.options, name)

        # Determine the default value for the option
        default_value = getattr(self.default_options, name, None)
        if default_value is None:
            raise ValueError(f"Option '{name}' not found in default options.")

        # If the option is set in command line arguments, use that
        if name in self.cmd_line_args:
            value = self.cmd_line_args[name]
            if isinstance(self.default_options, bool) and isinstance(value, str) and value.lower() in ('true', 'false'):
                value = value.lower() == 'true'
            setattr(self.options, name, value)
            self.prompted_options.add(name)
            return value

        # Replace placeholders in the default value
        if isinstance(default_value, str):
            contains_placeholder = "<" in default_value and ">" in default_value
            if contains_placeholder:
                has_dir_key = False

                def repl(match):
                    nonlocal has_dir_key
                    key = match.group(1)
                    transform_fn = match.group(2)
                    has_dir_key = has_dir_key or key.endswith("-dir")
                    key_surrounded_by_quotes = key.startswith('"') and key.endswith('"')
                    if key_surrounded_by_quotes:
                        val = key[1:-1]
                    else:
                        if key == 'os':
                            if self.is_windows():
                                val = "windows"
                            elif self.is_linux():
                                val = "linux"
                            elif self.is_macos():
                                val = "macos"
                            else:
                                raise ValueError("Unsupported operating system.")
                        else:
                            key = key.replace("-", "_")
                            val = getattr(self.options, key, None)

                    if transform_fn:
                        if transform_fn == "lower":
                            val = val.lower()
                        elif transform_fn == "upper":
                            val = val.upper()
                        elif transform_fn == "basename":
                            val = os.path.basename(val)
                        elif transform_fn.startswith("if(") and transform_fn.endswith(")"):
                            var_name = transform_fn[3:-1]
                            if getattr(self.options, var_name, None):
                                val = val.lower()
                            else:
                                val = ""
                    return val

                # Regex: <key> or <key:format>
                pattern = r"<([\"a-zA-Z0-9_\-]+)(?::([a-zA-Z0-9_\-\(\)]+))?>"
                default_value = re.sub(pattern, repl, default_value)
                if has_dir_key:
                    default_value = os.path.abspath(default_value)
                setattr(self.default_options, name, default_value)

        # If it's non-interactive, display and use the value directly
        if self.options.non_interactive:
            display = self.ui.maybe_shorten(default_value) if isinstance(default_value, str) else default_value
            self.ui.info(f"{prompt_text}: {display}")
            setattr(self.options, name, default_value)
            self.prompted_options.add(name)
            return default_value

        prompt = prompt_text
        # Prompt the user for the option value depending on the type
        if isinstance(getattr(self.default_options, name), bool):
            value = self.prompt_boolean(prompt, default_value)
        else:
            value = self.prompt_string(prompt, default_value)

        # Set the option and return the value
        setattr(self.options, name, value)
        self.prompted_options.add(name)
        return value

    def reprompt_option(self, name, prompt_text):
        return self.prompt_option(name, prompt_text, force_prompt=True)

    def prompt_build_type_option(self, name):
        value = self.prompt_option(name, "Build type")
        valid_build_types = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel", "OptimizedDebug", "DebugFast"]
        for t in valid_build_types:
            if t.lower().replace("-", "") == value.lower().replace("-", ""):
                if t == "DebugFast":
                    value = "DebugFast"
                setattr(self.options, name, t)
                return value
        print(f"Invalid build type '{value}'. Must be one of: {', '.join(valid_build_types)}.")
        value = self.reprompt_option(name, "Build type")
        for t in valid_build_types:
            if t.lower().replace("-", "") == value.lower().replace("-", ""):
                if t == "DebugFast":
                    value = "DebugFast"
                setattr(self.options, name, t)
                return value
        print(f"Invalid build type '{value}'. Must be one of: {', '.join(valid_build_types)}.")
        raise ValueError(f"Invalid build type '{value}'. Must be one of: {', '.join(valid_build_types)}.")

    def prompt_sanitizer_option(self, name):
        value = self.prompt_option(name, "Sanitizer (asan/ubsan/msan/tsan/none)")
        if not value:
            value = ''
            return value
        valid_sanitizers = ["ASan", "UBSan", "MSan", "TSan"]
        for t in valid_sanitizers:
            if t.lower() == value.lower():
                setattr(self.options, name, t)
                return value
        print(f"Invalid sanitizer '{value}'. Must be one of: {', '.join(valid_sanitizers)}.")
        value = self.reprompt_option(name, "Sanitizer (asan/ubsan/msan/tsan/none)")
        for t in valid_sanitizers:
            if t.lower() == value.lower():
                setattr(self.options, name, t)
                return value
        print(f"Invalid sanitizer '{value}'. Must be one of: {', '.join(valid_sanitizers)}.")
        raise ValueError(f"Invalid sanitizer '{value}'. Must be one of: {', '.join(valid_sanitizers)}.")

    def supports_ansi(self):
        return bool(self.ui.color_enabled)

    def run_cmd(self, cmd, cwd=None, tail=False):
        """
        Runs a shell command in the specified directory.
        When tail=True, only the last line of live output is shown (npm-style),
        while the full output is buffered and displayed only on failure.
        """
        if cwd is None:
            cwd = os.getcwd()
        display_cwd = self.ui.shorten_path(cwd) if cwd else os.getcwd()
        if isinstance(cmd, list):
            cmd_str = ' '.join(shlex.quote(arg) for arg in cmd)
        else:
            cmd_str = cmd
        # Always show the command with cwd for transparency
        self.ui.command(f"{display_cwd}> {cmd_str}", icon="💻")
        if self.options.dry_run:
            self.ui.info("dry-run: command not executed")
            return
        # Favor parallel builds unless user already set it
        effective_env = (self.env or os.environ).copy()
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in effective_env:
            try:
                effective_env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(max(1, os.cpu_count() or 1))
            except Exception:
                effective_env["CMAKE_BUILD_PARALLEL_LEVEL"] = "4"
        if not tail:
            try:
                r = subprocess.run(cmd, shell=isinstance(cmd, str), check=True, cwd=cwd, env=effective_env)
            except subprocess.CalledProcessError as exc:
                if self.options.debug:
                    raise
                tips = [
                    f"Working dir: {self.ui.shorten_path(cwd)}",
                ]
                if not self.options.verbose:
                    tips.append("Re-run with --verbose for full output")
                self.ui.error_block(f"Command failed: {exc}", tips)
                raise RuntimeError(f"Command '{cmd}' failed. Re-run with --debug for traceback.") from None
            if r.returncode != 0:
                raise RuntimeError(f"Command '{cmd}' failed with return code {r.returncode}.")
            return

        # tail == True: stream output but only show the last line live
        output_lines: List[str] = []
        try:
            proc = subprocess.Popen(
                cmd,
                shell=isinstance(cmd, str),
                cwd=cwd,
                env=effective_env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True,
            )
        except Exception as exc:  # noqa: BLE001
            raise RuntimeError(f"Failed to launch command '{cmd}': {exc}") from None

        try:
            term_width = shutil.get_terminal_size(fallback=(80, 24)).columns or 80
            prev_height = 0
            if proc.stdout:
                for line in proc.stdout:
                    line = line.rstrip("\r\n")
                    output_lines.append(line + "\n")
                    # compute how many terminal rows this line will wrap to
                    visible = line
                    height = max(1, math.ceil(len(visible) / term_width))
                    # move cursor up to start of previous render and clear those rows
                    if prev_height:
                        sys.stdout.write(f"\x1b[{prev_height}F")
                        for _ in range(prev_height):
                            sys.stdout.write("\x1b[2K\x1b[1E")
                        sys.stdout.write(f"\x1b[{prev_height}F")
                    # render current line (letting terminal wrap naturally)
                    sys.stdout.write("\x1b[2K" + line + "\n")
                    sys.stdout.flush()
                    prev_height = height
            proc.wait()
        finally:
            if proc.stdout:
                proc.stdout.close()

        if output_lines:
            # Ensure cursor ends on a clean line after the last render
            sys.stdout.write("\x1b[2K")
            sys.stdout.flush()

        if proc.returncode != 0:
            # On failure, show the full buffered output
            if not self.options.verbose:
                self.ui.error_block(
                    f"Command failed: {cmd}",
                    ["Working dir: " + self.ui.shorten_path(cwd or os.getcwd())],
                )
            print("".join(output_lines), end="")
            raise RuntimeError(f"Command '{cmd}' failed with return code {proc.returncode}.")

    def ensure_dir(self, path, exist_ok=True):
        if self.options.dry_run:
            self.ui.info(f"dry-run: would create directory {path}")
            return
        os.makedirs(path, exist_ok=exist_ok)

    def remove_dir(self, path):
        if not os.path.exists(path):
            return
        if self.options.dry_run:
            self.ui.info(f"dry-run: would remove directory {path}")
            return
        shutil.rmtree(path, ignore_errors=True)

    def write_text(self, path, content, encoding="utf-8"):
        if self.options.dry_run:
            self.ui.info(f"dry-run: would write file {path}")
            return
        parent = os.path.dirname(path)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(path, "w", encoding=encoding) as f:
            f.write(content)

    def clone_repo(self, repo, dest, branch=None, depth=None):
        """
        Clones a Git repository into the specified destination directory.
        :param repo: The URL of the repository to clone.
        :param dest: The destination directory where the repository will be cloned.
        :param branch: The branch or tag to checkout after cloning. Defaults to None (default branch).
        :param depth: The depth of the clone. Defaults to 1 (shallow clone).
        :return: None
        """
        cmd = [self.options.git_path, "-c", "core.symlinks=true", "clone"]
        if branch:
            cmd.extend(["--branch", branch])
        if depth:
            cmd.extend(["--depth", str(depth)])
        cmd.extend([repo, dest])
        self.run_cmd(cmd)

    def download_file(self, url, dest):
        """
        Downloads a file from the specified URL using Python's urllib.
        :param url: The URL of the file to download.
        :param dest: The destination file path where the file will be saved.
        :return: None
        """
        if self.options.dry_run:
            self.ui.info(f"  📥 dry-run: would download {self.ui.maybe_shorten(url)} -> {self.ui.shorten_path(dest)}")
            return
        if os.path.exists(dest):
            self.ui.info(f"File {self.ui.shorten_path(dest)} already exists. Skipping download.")
            return
        # Ensure the destination directory exists
        self.ensure_dir(os.path.dirname(dest))
        self.ui.info(f"  📥 Downloading")
        self.ui.kv("🌐 url", self.ui.maybe_shorten(url), key_width=9)
        self.ui.kv("📁 dest", self.ui.shorten_path(dest), key_width=9)
        urllib.request.urlretrieve(url, dest)

    def is_windows(self):
        """
        Checks if the current operating system is Windows.
        :return: bool: True if the OS is Windows, False otherwise.
        """
        return os.name == "nt"

    def is_linux(self):
        """
        Checks if the current operating system is Linux.
        :return: bool: True if the OS is Linux, False otherwise.
        """
        return os.name == "posix" and sys.platform.startswith("linux")

    def is_macos(self):
        """
        Checks if the current operating system is macOS.
        :return: bool: True if the OS is macOS, False otherwise.
        """
        return os.name == "posix" and sys.platform.startswith("darwin")

    def cmake_workflow(self, src_dir, build_type, build_dir, install_dir, extra_args=None, cc_flags=None,
                       cxx_flags=None, force_rebuild=False, remove_build_dir=True, allow_skip=True):
        """
        Configures and builds a CMake project.
        """

        # Check if we can skip the build
        if allow_skip and self.is_non_empty_dir(install_dir):
            if force_rebuild or self.prompt_option("force_rebuild", "Force rebuild deps"):
                print(f"Force rebuild requested. Removing existing install directory {install_dir}.")
                self.remove_dir(install_dir)
                if remove_build_dir and self.is_non_empty_dir(build_dir):
                    print(f"Removing existing build directory {build_dir}.")
                    self.remove_dir(build_dir)
            else:
                print(f"Install directory {install_dir} already exists and is not empty. Skipping build.")
                return

        if remove_build_dir and force_rebuild and self.is_non_empty_dir(build_dir):
            self.remove_dir(build_dir)
        if self.is_non_empty_dir(install_dir):
            self.remove_dir(install_dir)

        # Adjust any potential CMake flags from extra_args
        if cc_flags is None:
            cc_flags = ""
        if cxx_flags is None:
            cxx_flags = ""
        extra_args_remove_idx = []
        for i in range(0, len(extra_args or [])):
            extra_arg = extra_args[i]
            if extra_arg.startswith('-DCMAKE_C_FLAGS='):
                cc_flags += ' ' + extra_arg.split('=', 1)[1]
                extra_args_remove_idx.append(i)
            elif extra_arg.startswith('-DCMAKE_CXX_FLAGS='):
                cxx_flags += ' ' + extra_arg.split('=', 1)[1]
                extra_args_remove_idx.append(i)
            elif i != 0 and extra_args[i - 1].strip() == '-D':
                if extra_arg.startswith('CMAKE_C_FLAGS='):
                    cc_flags += ' ' + extra_arg.split('=', 1)[1]
                    extra_args_remove_idx.append(i - 1)
                    extra_args_remove_idx.append(i)
                elif extra_arg.startswith('CMAKE_CXX_FLAGS='):
                    cxx_flags += ' ' + extra_arg.split('=', 1)[1]
                    extra_args_remove_idx.append(i - 1)
                    extra_args_remove_idx.append(i)
        if extra_args_remove_idx:
            extra_args = [arg for i, arg in enumerate(extra_args or []) if i not in extra_args_remove_idx]

        config_args = [self.options.cmake_path, "-S", src_dir]

        if build_dir:
            config_args.extend(["-B", build_dir])
        if self.options.ninja_path:
            config_args.extend(["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={self.options.ninja_path}"])
        elif self.is_windows():
            generator = self.compiler_info.get("CMAKE_GENERATOR", "")
            if generator.startswith("Visual Studio"):
                config_args.extend(["-A", "x64"])

        if self.options.cc and self.options.cxx:
            config_args.extend(["-DCMAKE_C_COMPILER=" + self.options.cc,
                                "-DCMAKE_CXX_COMPILER=" + self.options.cxx])

        # If the cmake script happens to look for the python executable, we
        # already provide it on windows because it's often not in PATH.
        if self.is_windows():
            if self.options.python_path:
                config_args.extend(["-DPYTHON_EXECUTABLE=" + self.options.python_path])
            if self.options.git_path:
                config_args.extend(["-DGIT_EXECUTABLE=" + self.options.git_path])
                config_args.extend(["-DGIT_ROOT=" + os.path.dirname(self.options.git_path)])
                config_args.extend(["-DGit_ROOT=" + os.path.dirname(self.options.git_path)])

        # Maybe adjust build type based on the options for the main project
        if not self.is_abi_compatible(self.options.build_type, build_type):
            print(
                f"Warning: The build type '{build_type}' is not ABI compatible with the MrDocs build type '{self.options.build_type}'.")
            if self.options.build_type.lower() in ("debug", "debugfast", "debug-fast"):
                # User asked for Release dependency, so we do the best we can and change it to
                # an optimized debug build.
                print("Changing build type to 'OptimizedDebug' for ABI compatibility.")
                build_type = "OptimizedDebug"
            else:
                # User asked for a Debug dependency with Release build type for MrDocs.
                # The dependency should just copy the release type here. Other options wouldn't make sense
                # because we can't even debug it.
                print(f"Changing build type to '{self.options.build_type}' for ABI compatibility.")
                build_type = self.options.build_type

        # "OptimizedDebug" is not a valid build type. We interpret it as a special case
        # where the build type is Debug and optimizations are enabled.
        # This is equivalent to RelWithDebInfo on Unix, but ensures
        # Debug flags and the Debug ABI are used on Windows.
        build_type_is_optimizeddebug = build_type.lower() == 'optimizeddebug'
        cmake_build_type = build_type if not build_type_is_optimizeddebug else "Debug"
        if build_type:
            config_args.extend([f"-DCMAKE_BUILD_TYPE={cmake_build_type}"])
            if build_type_is_optimizeddebug:
                if self.is_windows():
                    cxx_flags += " /DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"
                    cc_flags += " /DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"
                else:
                    cxx_flags += " -Og -g"
                    cc_flags += " -Og -g"

        if isinstance(extra_args, list):
            config_args.extend(extra_args)
        else:
            raise TypeError(f"extra_args must be a list, got {type(extra_args)}.")

        cc_flags, cxx_flags = self._inject_clang_toolchain_flags(config_args, cc_flags, cxx_flags)

        if cc_flags:
            config_args.append(f"-DCMAKE_C_FLAGS={cc_flags.strip()}")
        if cxx_flags:
            config_args.append(f"-DCMAKE_CXX_FLAGS={cxx_flags.strip()}")
        cache_file = os.path.join(build_dir, "CMakeCache.txt")
        # Decide expected build file based on generator (default to Ninja if available)
        expected_build_file = os.path.join(build_dir, "build.ninja")
        gen = "ninja" if self.options.ninja_path else self.compiler_info.get("CMAKE_GENERATOR", "").lower()
        if "ninja" not in gen:
            expected_build_file = os.path.join(build_dir, "Makefile")
        needs_configure = force_rebuild or not (os.path.isfile(cache_file) and os.path.exists(expected_build_file))
        if needs_configure:
            # Configure step can be verbose; show last line live
            self.run_cmd(config_args, tail=True)

        # Always build; CMake will noop if nothing to do.
        build_args = [self.options.cmake_path, "--build", build_dir, "--config", cmake_build_type]
        # Use all available cores unless caller overrides via env/flags
        parallel_level = max(1, os.cpu_count() or 1)
        build_args.extend(["--parallel", str(parallel_level)])
        self.run_cmd(build_args, tail=True)

        install_args = [self.options.cmake_path, "--install", build_dir]
        if install_dir:
            install_args.extend(["--prefix", install_dir])
        if cmake_build_type:
            install_args.extend(["--config", cmake_build_type])
        self.run_cmd(install_args, tail=True)
        if remove_build_dir and self.prompt_option('remove_build_dir', 'Remove dep build dir'):
            print(f"Installation complete. Removing build directory {build_dir}.")
            self.remove_dir(build_dir)

    def is_executable(self, path):
        if not os.path.exists(path):
            return False
        if not os.path.isfile(path):
            return False
        if os.name == "nt":
            # On Windows, check for executable extensions
            _, ext = os.path.splitext(path)
            return ext.lower() in [".exe", ".bat", ".cmd", ".com"]
        else:
            return os.access(path, os.X_OK)

    def is_non_empty_dir(self, path):
        """
        Checks if the given path is a non-empty directory.
        :param path: The path to check.
        :return: bool: True if the path is a non-empty directory, False otherwise.
        """
        return os.path.exists(path) and os.path.isdir(path) and len(os.listdir(path)) > 0

    @lru_cache(maxsize=1)
    def get_vs_install_locations(self):
        if not self.is_windows():
            return []
        p = os.environ.get('ProgramFiles(x86)', r"C:\Program Files (x86)")
        path_vswhere = os.path.join(p,
                                    "Microsoft Visual Studio", "Installer", "vswhere.exe")
        if not self.is_executable(path_vswhere):
            return None
        cmd = [path_vswhere,
               "-latest", "-products", "*",
               "-requires", "Microsoft.Component.MSBuild",
               "-format", "json"]
        data = subprocess.check_output(cmd, universal_newlines=True)
        info = json.loads(data)
        if not info:
            return None
        return [inst.get("installationPath") for inst in info]

    def find_vs_tool(self, tool):
        if not self.is_windows():
            return None
        vs_tools = ["cmake", "ninja", "git", "python"]
        if tool not in vs_tools:
            return None
        vs_roots = self.get_vs_install_locations()
        for vs_root in vs_roots or []:
            ms_cext_path = os.path.join(vs_root, "Common7", "IDE", "CommonExtensions", "Microsoft")
            toolpaths = {
                'cmake': os.path.join(ms_cext_path, "CMake", "CMake", "bin", "cmake.exe"),
                'git': os.path.join(ms_cext_path, "TeamFoundation", "Team Explorer", "Git", "cmd", "git.exe"),
                'ninja': os.path.join(ms_cext_path, "CMake", "Ninja", "ninja.exe")
            }
            path = toolpaths.get(tool)
            if path and self.is_executable(path):
                return path
        return None

    def find_java(self):
        # 1. check JAVA_HOME env variable
        java_home = os.environ.get("JAVA_HOME")
        if java_home:
            exe = os.path.join(java_home, "bin", "java.exe")
            if os.path.isfile(exe):
                return exe

        # 2. check registry (64+32-bit)
        import winreg
        def reg_lookup(base, subkey):
            try:
                with winreg.OpenKey(base, subkey) as key:
                    ver, _ = winreg.QueryValueEx(key, "CurrentVersion")
                    key2 = winreg.OpenKey(base, subkey + "\\" + ver)
                    path, _ = winreg.QueryValueEx(key2, "JavaHome")
                    exe = os.path.join(path, "bin", "java.exe")
                    if os.path.isfile(exe):
                        return exe
            except OSError:
                return None

        for hive, sub in [
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\JavaSoft\Java Runtime Environment"),
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Wow6432Node\JavaSoft\Java Runtime Environment")
        ]:
            result = reg_lookup(hive, sub)
            if result:
                return result

        # 3. check common folders under Program Files
        for base in [os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")]:
            if not base:
                continue
            jroot = os.path.join(base, "Java")
            if os.path.isdir(jroot):
                for entry in os.listdir(jroot):
                    candidate = os.path.join(jroot, entry, "bin", "java.exe")
                    if os.path.isfile(candidate):
                        return candidate

        return None

    def find_tool(self, tool):
        # Environment variable {tool}_ROOT and {tool}_DIR
        env_suffixes = ["ROOT", "DIR", "PATH", "HOME", "INSTALL_DIR", "EXECUTABLE"]
        env_prefixes = [tool.upper(), tool.lower(), tool.title()]
        for env_prefix in env_prefixes:
            for env_suffix in env_suffixes:
                env_var = f"{env_prefix}_{env_suffix}"
                env_path = os.environ.get(env_var)
                if env_path and os.path.exists(env_path):
                    if self.is_executable(env_path):
                        return env_path
                    if os.path.isdir(env_path):
                        tool_filename = tool if tool.endswith(".exe") else tool + ".exe"
                        tool_path = os.path.join(env_path, tool_filename)
                        if self.is_executable(tool_path):
                            return tool_path
                        tool_bin_path = os.path.join(env_path, 'bin', tool_filename)
                        if self.is_executable(tool_bin_path):
                            return tool_bin_path

        # Look for the tool in the system PATH and special cases for Windows
        tool_path = shutil.which(tool)
        if not tool_path and self.is_windows():
            tool_path = self.find_vs_tool(tool)
            if not tool_path and tool == "java":
                tool_path = self.find_java()
        if not tool_path and tool == "python":
            tool_path = sys.executable
        return tool_path

    def check_tool(self, tool):
        """
        Checks if the required tools are available as a command line argument or
        the system PATH.

        If the path is available as a command line argument {tool}-path, we check if the tool really
        exists.

        If the path is not available as a command line argument, we check if the tool
        exists in the system PATH.

        If any of these checks fail, an error is raised indicating the missing tool.

        :return: None
        """
        default_value = self.find_tool(tool)
        if not default_value:
            default_value = tool
        setattr(self.default_options, f"{tool}_path", default_value)
        tool_path = self.prompt_option(f"{tool}_path", tool)
        if not self.is_executable(tool_path):
            raise FileNotFoundError(f"{tool} executable not found at {tool_path}.")

    def check_compilers(self):
        for option in ["cc", "cxx"]:
            self.prompt_option(option, option.replace("_", " "))
            if getattr(self.options, option):
                if not os.path.isabs(getattr(self.options, option)):
                    exec = shutil.which(getattr(self.options, option))
                    if exec is None:
                        raise FileNotFoundError(
                            f"{option} executable '{getattr(self.options, option)}' not found in PATH.")
                    setattr(self.options, option, exec)
                if not self.is_executable(getattr(self.options, option)):
                    raise FileNotFoundError(f"{option} executable not found at {getattr(self.options, option)}.")

    def check_tools(self):
        tools = ["git", "cmake", "python"]
        for tool in tools:
            self.check_tool(tool)

    def setup_source_dir(self):
        # Source dir is fixed to the repository containing this script; no prompts.
        self.options.source_dir = os.path.dirname(os.path.abspath(__file__))
        if not os.path.isdir(self.options.source_dir):
            raise NotADirectoryError(f"Source dir '{self.options.source_dir}' is not a directory.")
        # MrDocs build type
        self.prompt_build_type_option("build_type")
        self.prompt_sanitizer_option("sanitizer")
        if self.prompt_option("build_tests", "Build tests"):
            self.check_tool("java")

    def is_inside_mrdocs_dir(self, path):
        """
        Checks if the given path is inside the MrDocs source directory.
        :param path: The path to check.
        :return: bool: True if the path is inside the MrDocs source directory, False otherwise.
        """
        return os.path.commonpath([self.options.source_dir, path]) == self.options.source_dir

    def prompt_dependency_path_option(self, name, prompt_text):
        """
        Prompts the user for a dependency path option, ensuring it is not inside the MrDocs source directory.
        :param name: The name of the option to prompt for.
        :return: The value of the option after prompting the user.
        """
        self.prompt_option(name, prompt_text)
        value = getattr(self.options, name)
        value = os.path.abspath(value)
        setattr(self.options, name, value)
        if not os.path.exists(value):
            if not self.prompt_boolean(f"'{value}' does not exist. Create it?", True):
                raise FileNotFoundError(f"'{value}' does not exist and user chose not to create it.")
        return value

    def setup_third_party_dir(self):
        self.prompt_dependency_path_option("third_party_src_dir", "3rd-party root (src/build/install)")
        self.ensure_dir(self.options.third_party_src_dir)

    @lru_cache(maxsize=1)
    def probe_compilers(self):
        print("Probing default system compilers...")
        variables = []
        for lang in ["C", "CXX"]:
            for suffix in ["COMPILER", "COMPILER_ID", "COMPILER_VERSION", "COMPILER_AR", "COMPILER_LINKER",
                           "COMPILER_LINKER_ID", "COMPILER_ABI"]:
                variables.append(f"CMAKE_{lang}_{suffix}")
        variables.append("CMAKE_GENERATOR")

        probe_dir = os.path.join(self.options.third_party_src_dir, "cmake-probe")
        if self.options.dry_run:
            self.ui.info("dry-run: would probe compilers via CMake")
            return
        if os.path.exists(probe_dir):
            self.remove_dir(probe_dir)
        self.ensure_dir(probe_dir)

        # Create minimal CMakeLists.txt
        cmake_lists = [
            "cmake_minimum_required(VERSION 3.10)",
            "project(probe C CXX)"
        ]
        for var in variables:
            cmake_lists.append(f'message(STATUS "{var}=${{{var}}}")')
        self.write_text(os.path.join(probe_dir, "CMakeLists.txt"), "\n".join(cmake_lists))

        # Build command
        cmd = [self.options.cmake_path, "-S", probe_dir]
        env = os.environ.copy()
        if self.options.cc:
            cmd += ["-DCMAKE_C_COMPILER=" + self.options.cc]
        if self.options.cxx:
            cmd += ["-DCMAKE_CXX_COMPILER=" + self.options.cxx]
        cmd += ["-B", os.path.join(probe_dir, "build")]

        # Run cmake and capture output
        result = subprocess.run(cmd, env=env, text=True, capture_output=True)
        if result.returncode != 0:
            raise RuntimeError(f"CMake failed:\n{result.stdout}\n{result.stderr}")

        # Parse values from lines like: "-- VAR=value"
        values = {}
        for line in result.stdout.splitlines():
            if line.startswith("-- "):
                for var in variables:
                    prefix = f"{var}="
                    if prefix in line:
                        values[var] = line.split(prefix, 1)[1].strip()

        # Store this map in self for later use
        self.compiler_info = values

        # Clean up probe directory
        self.remove_dir(probe_dir)

        # Print default C++ compiler path
        print(
            f"Default C++ compiler: {self.compiler_info.get('CMAKE_CXX_COMPILER_ID', 'unknown')} ({self.compiler_info.get('CMAKE_CXX_COMPILER', 'unknown')})")

    # --------------------------
    # Recipe-driven dependencies
    # --------------------------
    def build_archive_url(self, url, ref):
        """
        For GitHub URLs, return an archive download URL for a commit or tag.
        """
        if "github.com" not in url or not ref:
            return None
        # strip .git and trailing slash
        clean = url
        if clean.endswith(".git"):
            clean = clean[:-4]
        clean = clean.rstrip("/")
        parts = clean.split("github.com/", 1)[1].split("/")
        if len(parts) < 2:
            return None
        owner, repo = parts[0], parts[1]
        return f"https://github.com/{owner}/{repo}/archive/{ref}.zip"

    def extract_zip_flatten(self, zip_path, dest_dir):
        if self.options.dry_run:
            self.ui.info(f"dry-run: would extract {zip_path} into {dest_dir}")
            return
        with zipfile.ZipFile(zip_path, 'r') as zf:
            infos = zf.infolist()
            # determine top-level prefix
            prefix = None
            for info in infos:
                name = info.filename
                if name.endswith("/"):
                    continue
                parts = name.split("/", 1)
                if len(parts) == 2:
                    prefix = parts[0] + "/"
                    break
            if prefix is None:
                prefix = ""
            for info in infos:
                name = info.filename
                if name.endswith("/"):
                    continue
                rel = name[len(prefix):] if name.startswith(prefix) else name
                target_path = os.path.join(dest_dir, rel)
                target_dir = os.path.dirname(target_path)
                self.ensure_dir(target_dir)
                with zf.open(info, 'r') as src, open(target_path, 'wb') as dst:
                    shutil.copyfileobj(src, dst)

    def extract_tar_flatten(self, tar_path, dest_dir):
        if self.options.dry_run:
            self.ui.info(f"dry-run: would extract {tar_path} into {dest_dir}")
            return
        mode = "r:*"
        with tarfile.open(tar_path, mode) as tf:
            # determine top-level prefix
            prefix = None
            for member in tf.getmembers():
                parts = member.name.split("/", 1)
                if len(parts) == 2:
                    prefix = parts[0] + "/"
                    break
            if prefix is None:
                prefix = ""
            for member in tf.getmembers():
                if member.isdir():
                    continue
                rel = member.name[len(prefix):] if member.name.startswith(prefix) else member.name
                target_path = os.path.join(dest_dir, rel)
                self.ensure_dir(os.path.dirname(target_path))
                with tf.extractfile(member) as src, open(target_path, "wb") as dst:
                    shutil.copyfileobj(src, dst)

    def recipe_stamp_path(self, recipe: Recipe):
        return os.path.join(recipe.install_dir, ".bootstrap-stamp.json")

    def is_recipe_up_to_date(self, recipe: Recipe, resolved_ref: str):
        stamp_path = self.recipe_stamp_path(recipe)
        if not os.path.exists(stamp_path):
            return False
        try:
            data = json.loads(open(stamp_path, "r", encoding="utf-8").read())
        except Exception:
            return False
        return data.get("version") == recipe.version and data.get("ref") == resolved_ref

    def write_recipe_stamp(self, recipe: Recipe, resolved_ref: str):
        if self.options.dry_run:
            self.ui.info(f"dry-run: would write stamp for {recipe.name} at {self.recipe_stamp_path(recipe)}")
            return
        payload = {
            "name": recipe.name,
            "version": recipe.version,
            "ref": resolved_ref,
        }
        self.ensure_dir(recipe.install_dir)
        self.write_text(self.recipe_stamp_path(recipe), json.dumps(payload, indent=2))

    def fetch_recipe_source(self, recipe: Recipe):
        src = recipe.source
        dest = recipe.source_dir
        archive_url = None
        resolved_ref = src.commit or src.tag or src.branch or src.ref or ""

        if self.options.clean and os.path.exists(dest):
            self.remove_dir(dest)
        if not self.options.force and self.is_recipe_up_to_date(recipe, resolved_ref):
            self.ui.ok(f"[{recipe.name}] already up to date ({resolved_ref or 'HEAD'}).")
            return resolved_ref
        # If source already exists and we're not forcing or cleaning, skip re-download
        if os.path.isdir(dest) and not self.options.clean and not self.options.force:
            self.ui.info(f"{recipe.name}: source already present at {self.ui.shorten_path(dest)}; skipping download.")
            return resolved_ref or "HEAD"

        if src.type == "git":
            archive_url = self.build_archive_url(src.url, src.commit or src.tag or src.ref)
        elif src.type in ("archive", "http", "zip"):
            archive_url = src.url

        if archive_url:
            filename = os.path.basename(archive_url.split("?")[0])
            tmp_archive = os.path.join(self.options.source_dir, "build", "third-party", "source", filename)
            self.download_file(archive_url, tmp_archive)
            if not self.options.dry_run and os.path.exists(dest):
                self.remove_dir(dest)
            self.ensure_dir(dest)
            if not self.options.dry_run:
                if archive_url.endswith(".zip"):
                    self.extract_zip_flatten(tmp_archive, dest)
                else:
                    self.extract_tar_flatten(tmp_archive, dest)
                os.remove(tmp_archive)
        else:
            # fallback to git
            depth = ["--depth", str(src.depth)] if src.depth else []
            if not os.path.exists(dest):
                self.ensure_dir(os.path.dirname(dest))
                clone_cmd = [self.options.git_path or "git", "clone", src.url, dest, *depth]
                if src.branch and not src.commit:
                    clone_cmd.extend(["--branch", src.branch])
                self.run_cmd(clone_cmd)
            if resolved_ref:
                self.run_cmd([self.options.git_path or "git", "fetch", "--tags"], cwd=dest)
                self.run_cmd([self.options.git_path or "git", "checkout", resolved_ref], cwd=dest)
            else:
                self.run_cmd([self.options.git_path or "git", "pull", "--ff-only"], cwd=dest)

        return resolved_ref or "HEAD"

    def apply_recipe_patches(self, recipe: Recipe):
        patch_root = os.path.join(self.patches_dir, recipe.name)
        if not os.path.isdir(patch_root):
            return
        entries = sorted(os.listdir(patch_root))
        for entry in entries:
            path = os.path.join(patch_root, entry)
            if entry.endswith(".patch"):
                self.ui.info(f"Applying patch {path}")
                self.run_cmd(["patch", "-p1", "-i", path], cwd=recipe.source_dir)
            else:
                target = os.path.join(recipe.source_dir, entry)
                if os.path.isdir(path):
                    if self.options.dry_run:
                        self.ui.info(f"dry-run: would copy directory {path} -> {target}")
                    else:
                        shutil.copytree(path, target, dirs_exist_ok=True)
                else:
                    if self.options.dry_run:
                        self.ui.info(f"dry-run: would copy file {path} -> {target}")
                    else:
                        self.ensure_dir(os.path.dirname(target))
                        shutil.copy(path, target)

    def _expand_path(self, template: str, build_type: str):
        if not template:
            return template
        mrdocs = self.options.source_dir
        third = self.options.third_party_src_dir
        build_lower = build_type.lower() if build_type else ""
        repl = {
            "${source_dir}": mrdocs,
            "${third_party_src_dir}": third,
            "${build_type}": build_type,
            "${build_type_lower}": build_lower,
        }
        out = template
        for k, v in repl.items():
            out = out.replace(k, v)
        if not os.path.isabs(out):
            out = os.path.normpath(os.path.join(mrdocs, out))
        return out

    def load_recipe_files(self) -> List[Recipe]:
        recipes_dir = self.recipes_dir
        patches_dir = self.patches_dir
        if not os.path.isdir(recipes_dir):
            return []
        # For debug-fast, dependencies reuse release (or optimized debug on Windows) builds/presets.
        dep_build_type = "OptimizedDebug" if self.is_windows() else "Release"
        dep_preset = self.options.preset
        if self.options.build_type.lower() in ("debugfast", "debug-fast"):
            if "debug-fast" in dep_preset:
                dep_preset = dep_preset.replace("debug-fast", dep_build_type.lower())
            elif "debugfast" in dep_preset:
                dep_preset = dep_preset.replace("debugfast", dep_build_type.lower())
        recipes: List[Recipe] = []
        for path in sorted(os.listdir(recipes_dir)):
            if not path.endswith(".json"):
                continue
            full = os.path.join(recipes_dir, path)
            try:
                data = json.load(open(full, "r", encoding="utf-8"))
            except Exception as exc:
                self.ui.warn(f"Skipping recipe {path}: {exc}")
                continue
            src = data.get("source", {})
            recipe = Recipe(
                name=data.get("name") or os.path.splitext(path)[0],
                version=str(data.get("version", "")),
                source=RecipeSource(
                    type=src.get("type", "git"),
                    url=src.get("url", ""),
                    branch=src.get("branch"),
                    tag=src.get("tag"),
                    commit=src.get("commit"),
                    ref=src.get("ref"),
                    depth=src.get("depth"),
                    submodules=bool(src.get("submodules", False)),
                ),
                dependencies=data.get("dependencies", []),
                source_dir=data.get("source_dir", ""),
                build_dir=data.get("build_dir", ""),
                install_dir=data.get("install_dir", ""),
                build_type=data.get("build_type", "Release"),
                build=data.get("build", []),
                tags=data.get("tags", []),
                package_root_var=data.get("package_root_var"),
                install_scope=data.get("install_scope", "per-preset"),
            )
            placeholders = self._recipe_placeholders(recipe)

            # Apply placeholders to source reference fields
            recipe.source.url = self._apply_placeholders(recipe.source.url, placeholders)
            recipe.source.branch = self._apply_placeholders(recipe.source.branch, placeholders)
            recipe.source.tag = self._apply_placeholders(recipe.source.tag, placeholders)
            recipe.source.commit = self._apply_placeholders(recipe.source.commit, placeholders)
            recipe.source.ref = self._apply_placeholders(recipe.source.ref, placeholders)

            # Paths are controlled by bootstrap, not the recipe file.
            tp_root = os.path.join(self.options.source_dir, "build", "third-party")
            preset = dep_preset
            if recipe.install_scope == "global":
                recipe.source_dir = os.path.join(tp_root, "source", recipe.name)
                recipe.build_dir = os.path.join(tp_root, "build", recipe.name)
                recipe.install_dir = os.path.join(tp_root, "install", recipe.name)
            else:
                recipe.source_dir = os.path.join(tp_root, "source", recipe.name)
                recipe.build_dir = os.path.join(tp_root, "build", preset, recipe.name)
                recipe.install_dir = os.path.join(tp_root, "install", preset, recipe.name)
            recipes.append(recipe)
        return recipes

    def _topo_sort_recipes(self, recipes: List[Recipe]) -> List[Recipe]:
        by_name = {r.name: r for r in recipes}
        visited: Dict[str, bool] = {}
        order: List[Recipe] = []

        def visit(name, stack):
            state = visited.get(name)
            if state is True:
                return
            if state is False:
                raise RuntimeError(f"Dependency cycle: {' -> '.join(stack + [name])}")
            visited[name] = False
            stack.append(name)
            for dep in by_name[name].dependencies:
                if dep not in by_name:
                    raise RuntimeError(f"Missing dependency recipe '{dep}' needed by '{name}'")
                visit(dep, stack)
            visited[name] = True
            stack.pop()
            order.append(by_name[name])

        for n in by_name:
            if visited.get(n) is not True:
                visit(n, [])
        return order

    def _recipe_placeholders(self, recipe: Recipe) -> Dict[str, str]:
        host_suffix = "windows" if self.is_windows() else "unix"
        return {
            "BOOTSTRAP_BUILD_TYPE": recipe.build_type,
            "BOOTSTRAP_BUILD_TYPE_LOWER": recipe.build_type.lower(),
            "BOOTSTRAP_CONFIGURE_PRESET": self.options.preset,
            "BOOTSTRAP_CC": self.options.cc or "",
            "BOOTSTRAP_CXX": self.options.cxx or "",
            "BOOTSTRAP_PROJECT_BUILD_DIR": self.options.build_dir,
            "BOOTSTRAP_PROJECT_INSTALL_DIR": self.options.install_dir,
            "BOOTSTRAP_HOST_PRESET_SUFFIX": host_suffix,
            "build_type": recipe.build_type,
            "build_type_lower": recipe.build_type.lower(),
        }

    def _apply_placeholders(self, value: Any, placeholders: Dict[str, str]) -> Any:
        if isinstance(value, str):
            for k, v in placeholders.items():
                value = value.replace("${" + k + "}", v)
            return value
        if isinstance(value, list):
            return [self._apply_placeholders(v, placeholders) for v in value]
        if isinstance(value, dict):
            return {self._apply_placeholders(k, placeholders): self._apply_placeholders(v, placeholders) for k, v in value.items()}
        return value

    def _run_cmake_recipe_step(self, recipe: Recipe, step: Dict[str, Any]):
        cmake_exe = shutil.which("cmake")
        if not cmake_exe:
            raise RuntimeError("cmake executable not found in PATH.")
        placeholders = self._recipe_placeholders(recipe)
        opts = self._apply_placeholders(step.get("options", []), placeholders)
        build_dir = self._expand_path(step.get("build_dir", recipe.build_dir), recipe.build_type)
        source_dir = self._expand_path(step.get("source_dir", recipe.source_dir), recipe.build_type)
        source_subdir = step.get("source_subdir")
        if source_subdir:
            source_dir = os.path.join(source_dir, self._apply_placeholders(source_subdir, placeholders))
        generator = step.get("generator")
        config = self._apply_placeholders(step.get("config", recipe.build_type), placeholders)
        targets = self._apply_placeholders(step.get("targets", []), placeholders)
        install_flag = step.get("install", True)

        # Optional sanitizer-specific options declared in the recipe
        san_map = step.get("sanitizers", {})
        if self.options.sanitizer:
            san = self.options.sanitizer.lower()
            if san_map:
                extra = san_map.get(san)
                if extra is None:
                    raise ValueError(f"Unknown sanitizer '{self.options.sanitizer}' for recipe '{recipe.name}'.")
                extra_opts = self._apply_placeholders(extra, placeholders)
                if isinstance(extra_opts, list):
                    opts.extend(extra_opts)
                else:
                    opts.append(extra_opts)
            else:
                # Fallback: apply typical compiler sanitizer flags (data-driven by compiler/sanitizer)
                if self.is_windows():
                    msvc_flags = {
                        "asan": "/fsanitize=address",
                    }
                    flag = msvc_flags.get(san)
                else:
                    posix_flags = {
                        "asan": "-fsanitize=address",
                        "ubsan": "-fsanitize=undefined",
                        "msan": "-fsanitize=memory",
                        "tsan": "-fsanitize=thread",
                    }
                    flag = posix_flags.get(san)

                if flag:
                    # Initialize build/link flags; use *_FLAGS_INIT to avoid clobbering cache if present
                    opts.extend([
                        f"-DCMAKE_C_FLAGS_INIT={flag}",
                        f"-DCMAKE_CXX_FLAGS_INIT={flag}",
                        f"-DCMAKE_EXE_LINKER_FLAGS_INIT={flag}",
                        f"-DCMAKE_SHARED_LINKER_FLAGS_INIT={flag}",
                    ])

        self.ensure_dir(build_dir)
        cfg_cmd = [cmake_exe, "-S", source_dir, "-B", build_dir]
        if generator:
            cfg_cmd.extend(["-G", generator])
        cfg_cmd.append(f"-DCMAKE_BUILD_TYPE={config}")
        cfg_cmd.append(f"-DCMAKE_INSTALL_PREFIX={recipe.install_dir}")
        if self.options.cc:
            cfg_cmd.append(f"-DCMAKE_C_COMPILER={self.options.cc}")
        if self.options.cxx:
            cfg_cmd.append(f"-DCMAKE_CXX_COMPILER={self.options.cxx}")
        cfg_cmd.extend(opts)
        # Configure step can be chatty; use tail view
        self.run_cmd(cfg_cmd, tail=True)

        build_cmd = [cmake_exe, "--build", build_dir]
        if config:
            build_cmd.extend(["--config", config])
        if targets:
            build_cmd.extend(["--target", *targets])
        # Use available cores unless caller specified parallelism via env/flags
        if "--parallel" not in build_cmd:
            try:
                parallel_level = max(1, os.cpu_count() or 1)
                build_cmd.extend(["--parallel", str(parallel_level)])
            except Exception:
                pass
        if self.options.force:
            build_cmd.extend(["--clean-first"])
        self.run_cmd(build_cmd, tail=True)

        if install_flag:
            inst_cmd = [cmake_exe, "--install", build_dir]
            if config:
                inst_cmd.extend(["--config", config])
            self.run_cmd(inst_cmd, tail=True)

    def _run_command_recipe_step(self, recipe: Recipe, step: Dict[str, Any]):
        placeholders = self._recipe_placeholders(recipe)
        command = self._apply_placeholders(step.get("command", []), placeholders)
        cwd = step.get("cwd")
        if cwd:
            cwd = self._expand_path(self._apply_placeholders(cwd, placeholders), recipe.build_type)
        env = step.get("env")
        if env:
            env = {k: self._apply_placeholders(v, placeholders) for k, v in env.items()}
            env.update(self.env or {})
        self.run_cmd(command, cwd=cwd)

    def build_recipe(self, recipe: Recipe):
        for raw_step in (recipe.build or []):
            step_type = raw_step.get("type", "").lower()
            if step_type == "cmake":
                self._run_cmake_recipe_step(recipe, raw_step)
            elif step_type == "command":
                self._run_command_recipe_step(recipe, raw_step)
            else:
                raise RuntimeError(f"Unsupported build step type '{step_type}' in recipe '{recipe.name}'")

    def install_recipes(self):
        recipe_list = self.load_recipe_files()
        if not recipe_list:
            raise RuntimeError(f"No recipes found in {self.recipes_dir}. Add recipe JSON files to proceed.")

        if self.options.recipe_filter:
            wanted = {name.strip().lower() for name in self.options.recipe_filter.split(",") if name.strip()}
            recipe_list = [r for r in recipe_list if r.name.lower() in wanted]

        ordered = self._topo_sort_recipes(recipe_list)

        def detect_root_var(recipe: Recipe) -> Optional[str]:
            # Prefer an inferred name from installed *Config.cmake (matches actual package case)
            cfg_name = None
            for dirpath, _, filenames in os.walk(recipe.install_dir):
                for fn in filenames:
                    if fn.endswith("Config.cmake"):
                        cfg_name = fn[:-len("Config.cmake")]
                        break
                if cfg_name:
                    break
            if cfg_name:
                return f"{cfg_name}_ROOT"
            # Fallback to recipe hint if no config found
            if recipe.package_root_var:
                return recipe.package_root_var
            return None

        for recipe in ordered:
            resolved_ref = self.fetch_recipe_source(recipe)
            self.apply_recipe_patches(recipe)
            # Track recipe metadata
            self.recipe_info[recipe.name] = recipe
            root_var = detect_root_var(recipe)
            if root_var:
                self.package_roots[root_var] = recipe.install_dir
            if self.options.skip_build:
                continue
            if self.is_recipe_up_to_date(recipe, resolved_ref) and not self.options.force:
                self.ui.ok(f"[{recipe.name}] up to date; skipping build.")
                continue
            self.build_recipe(recipe)
            self.write_recipe_stamp(recipe, resolved_ref)
            root_var = detect_root_var(recipe)
            if root_var:
                self.package_roots[root_var] = recipe.install_dir
            
        print(f"Default C++ build system: {self.compiler_info.get('CMAKE_GENERATOR', 'unknown')}")

    def show_preset_summary(self):
        """Display key details of the selected CMake user preset."""
        path = os.path.join(self.options.source_dir, "CMakeUserPresets.json")
        try:
            data = json.load(open(path, "r", encoding="utf-8"))
        except Exception as exc:
            self.ui.warn(f"Could not read {self.ui.shorten_path(path)}: {exc}")
            return
        preset = None
        for p in data.get("configurePresets", []):
            if p.get("name") == self.options.preset:
                preset = p
                break
        if not preset:
            self.ui.warn(f"Preset '{self.options.preset}' not found in {self.ui.shorten_path(path)}")
            return
        cache = preset.get("cacheVariables", {})
        roots = {k: v for k, v in cache.items() if k.endswith("_ROOT")}
        summary = [
            ("Preset file", self.ui.shorten_path(path)),
            ("Preset name", preset.get("name", "")),
            ("Generator", preset.get("generator", "")),
            ("Binary dir", preset.get("binaryDir", "")),
        ]
        if roots:
            for k, v in sorted(roots.items()):
                summary.append((k, v))
        if "CMAKE_MAKE_PROGRAM" in cache:
            summary.append(("CMAKE_MAKE_PROGRAM", cache["CMAKE_MAKE_PROGRAM"]))
        self.ui.kv_block(None, summary, indent=4)

    @lru_cache(maxsize=1)
    def probe_msvc_dev_env(self):
        if not self.is_windows():
            return
        print("Probing MSVC development environment variables...")
        vs_roots = self.get_vs_install_locations()
        vcvarsall_path = None
        for vs_root in vs_roots or []:
            vcvarsall_path_candidate = os.path.join(vs_root, "VC", "Auxiliary", "Build", "vcvarsall.bat")
            if os.path.exists(vcvarsall_path_candidate):
                vcvarsall_path = vcvarsall_path_candidate
                print(f"Found vcvarsall.bat at {vcvarsall_path}.")
                break
        if not vcvarsall_path:
            print("No vcvarsall.bat found. MSVC development environment variables will not be set.")
            return
        # Run it with x64 argument and VSCMD_DEBUG=2 set and get the output
        cmd = [vcvarsall_path, "x64"]
        env = os.environ.copy()
        env["VSCMD_DEBUG"] = "2"
        result = subprocess.run(cmd, env=env, text=True, capture_output=True, shell=True)
        # print the output
        if result.returncode != 0:
            raise RuntimeError(f"vcvarsall.bat failed:\n{result.stdout}\n{result.stderr}")

        # Get the lines in the output between the two lines that among other things say
        post_env = {}
        in_post_init_header = False
        for line in result.stdout.splitlines():
            contains_post_init_header = "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------" in line
            if contains_post_init_header:
                if in_post_init_header:
                    break
                in_post_init_header = True
                continue
            if not in_post_init_header:
                continue
            if '=' in line:
                key, value = line.split('=', 1)
                post_env[key.strip()] = value.strip()
        if not in_post_init_header or not post_env:
            print("No post-init environment variables found in vcvarsall.bat output.")
            return

        # Populate the environment with anything that changed, and later use it
        # for any other commands.
        self.env = os.environ.copy()
        for key, value in post_env.items():
            if key not in self.env:
                print(f"* Inserting {key}={value}")
            elif value != self.env[key]:
                print(f"* Updating {key}={value}")
            self.env[key] = value
        print("MSVC development environment variables extracted successfully.")

    def _inject_clang_toolchain_flags(self, config_args: List[str], cc_flags: str, cxx_flags: str):
        """
        For clang/LLVM toolchains, prefer colocated binutils/linker/libc++ if available.
        Works for Homebrew or any LLVM install that keeps tools together.
        """
        self.probe_compilers()
        compiler_id = self.compiler_info.get("CMAKE_CXX_COMPILER_ID", "").lower()
        if compiler_id not in ("clang", "appleclang"):
            return cc_flags, cxx_flags

        cxx_path = self.options.cxx or self.compiler_info.get("CMAKE_CXX_COMPILER", "")
        if not cxx_path:
            return cc_flags, cxx_flags

        tool_root = os.path.abspath(os.path.join(os.path.dirname(cxx_path), os.pardir))
        bin_dir = os.path.join(tool_root, "bin")

        def maybe_append(flag_var, tool_name):
            tool_path = os.path.join(bin_dir, tool_name)
            if self.is_executable(tool_path):
                config_args.append(f"-D{flag_var}={tool_path}")

        for var, tool in [
            ("CMAKE_AR", "llvm-ar"),
            ("CMAKE_CXX_COMPILER_AR", "llvm-ar"),
            ("CMAKE_C_COMPILER_AR", "llvm-ar"),
            ("CMAKE_RANLIB", "llvm-ranlib"),
        ]:
            maybe_append(var, tool)

        for linker in ["ld.lld", "lld"]:
            ld_path = os.path.join(bin_dir, linker)
            if self.is_executable(ld_path):
                config_args.append(f"-DCMAKE_C_COMPILER_LINKER={ld_path}")
                config_args.append(f"-DCMAKE_CXX_COMPILER_LINKER={ld_path}")
                break

        libcxx_include = os.path.join(tool_root, "include", "c++", "v1")
        libcxx_lib = os.path.join(tool_root, "lib", "c++")
        libunwind = os.path.join(tool_root, "lib", "unwind")
        if os.path.exists(libcxx_include) and os.path.exists(libcxx_lib):
            cxx_flags += f" -stdlib=libc++ -I{libcxx_include}"
            ld_flags = f"-L{libcxx_lib}"
            if os.path.exists(libunwind):
                ld_flags += f" -L{libunwind} -lunwind"
            if self.options.sanitizer:
                ld_flags += f" -fsanitize={self.sanitizer_flag_name(self.options.sanitizer)}"
            for var in ["CMAKE_EXE_LINKER_FLAGS", "CMAKE_SHARED_LINKER_FLAGS", "CMAKE_MODULE_LINKER_FLAGS"]:
                config_args.append(f"-D{var}={ld_flags}")

        return cc_flags, cxx_flags

    def install_ninja(self):
        # 1. Check if the user has set a ninja_path option
        if self.prompt_option("ninja_path", "ninja"):
            if not os.path.isabs(self.options.ninja_path):
                self.options.ninja_path = self.find_tool(self.options.ninja_path)
            if not self.is_executable(self.options.ninja_path):
                raise FileNotFoundError(f"Ninja executable not found at {self.options.ninja_path}.")
            return

        # 2. If ninja_path is not set, but does the user have it available in PATH?
        ninja_path = self.find_tool("ninja")
        if ninja_path:
            self.ui.info(f"Ninja found in PATH at {ninja_path}. Using it.")
            self.options.ninja_path = ninja_path
            return

        # 3. Ninja path isn't set and not available in PATH, so we download it
        tp_root = os.path.join(self.options.source_dir, "build", "third-party")
        source_dir = os.path.join(tp_root, "source", "ninja")
        install_dir = os.path.join(tp_root, "install", self.options.preset, "ninja")
        self.ensure_dir(source_dir)
        self.ensure_dir(install_dir)
        ninja_dir = install_dir
        exe_name = 'ninja.exe' if platform.system().lower() == 'windows' else 'ninja'
        ninja_path = os.path.join(ninja_dir, exe_name)
        if os.path.exists(ninja_path) and self.is_executable(ninja_path):
            try:
                rel = os.path.relpath(ninja_path, self.options.source_dir)
                display_path = "./" + rel if not rel.startswith("..") else ninja_path
            except Exception:
                display_path = ninja_path
            self.ui.ok(f"[ninja] already available at {display_path}; reusing.")
            self.options.ninja_path = ninja_path
            return

        # 3a. Determine the ninja asset name based on the platform and architecture
        system = platform.system().lower()
        arch = platform.machine().lower()
        if system == 'linux':
            if arch in ('aarch64', 'arm64'):
                asset_name = 'ninja-linux-aarch64.zip'
            else:
                asset_name = 'ninja-linux.zip'
        elif system == 'darwin':
            asset_name = 'ninja-mac.zip'
        elif system == 'windows':
            if arch in ('arm64', 'aarch64'):
                asset_name = 'ninja-winarm64.zip'
            else:
                asset_name = 'ninja-win.zip'
        else:
            return

        destination_dir = source_dir
        # 3b. Find the download URL for the latest Ninja release asset
        api_url = 'https://api.github.com/repos/ninja-build/ninja/releases/latest'
        if self.options.dry_run:
            self.options.ninja_path = ninja_path
            self.ui.info(f"dry-run: would fetch {api_url} and download {asset_name} -> {destination_dir}")
            return
        with urllib.request.urlopen(api_url) as resp:
            data = json.load(resp)
        release_assets = data.get('assets', [])
        download_url = None
        for a in release_assets:
            if a.get('name') == asset_name:
                download_url = a.get('browser_download_url')
                break
        if not download_url:
            # Could not find release asset named asset_name
            return

        # 3c. Download the asset to the third-party source directory
        tmpzip = os.path.join(destination_dir, asset_name)
        self.ensure_dir(destination_dir)
        print(f'Downloading {asset_name} …')
        urllib.request.urlretrieve(download_url, tmpzip)

        # 3d. Extract the downloaded zip file into the ninja dir
        print('Extracting…')
        self.ensure_dir(ninja_dir)
        with zipfile.ZipFile(tmpzip, 'r') as z:
            z.extractall(ninja_dir)
        os.remove(tmpzip)

        # 3e. Set the ninja_path option to the extracted ninja executable
        if platform.system().lower() != 'windows':
            os.chmod(ninja_path, 0o755)
        self.options.ninja_path = ninja_path

    def sanitizer_flag_name(self, sanitizer):
        """
        Returns the flag name for the given sanitizer.
        :param sanitizer: The sanitizer name (ASan, UBSan, MSan, TSan).
        :return: str: The flag name for the sanitizer.
        """
        if sanitizer.lower() == "asan":
            return "address"
        elif sanitizer.lower() == "ubsan":
            return "undefined"
        elif sanitizer.lower() == "msan":
            return "memory"
        elif sanitizer.lower() == "tsan":
            return "thread"
        else:
            raise ValueError(f"Unknown sanitizer '{sanitizer}'.")

    def is_abi_compatible(self, build_type_a, build_type_b):
        if not self.is_windows():
            return True
        # On Windows, Debug and Release builds are not ABI compatible
        def _is_debug(bt):
            return bt.lower() in ("debug", "debugfast", "debug-fast", "optimizeddebug")
        build_type_a_is_debug = _is_debug(build_type_a)
        build_type_b_is_debug = _is_debug(build_type_b)
        return build_type_a_is_debug == build_type_b_is_debug


    def create_cmake_presets(self):
        # Generate or update CMakeUserPresets.json directly
        user_presets_path = os.path.join(self.options.source_dir, "CMakeUserPresets.json")
        if os.path.exists(user_presets_path):
            with open(user_presets_path, "r") as f:
                user_presets = json.load(f)
        else:
            user_presets = {
                "version": 6,
                "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
                "configurePresets": []
            }

        # Come up with a nice user preset name
        self.prompt_option("preset", "CMake preset")

        # Upsert the preset in the "configurePresets" array of objects
        # If preset with the same name already exists, we update it
        # If a preset with the same name does not exist, we create it
        hostSystemName = "Windows"
        if os.name == "posix":
            if os.uname().sysname == "Darwin":
                hostSystemName = "Darwin"
            else:
                hostSystemName = "Linux"
        OSDisplayName = hostSystemName
        if OSDisplayName == "Darwin":
            OSDisplayName = "macOS"

        # Preset inherits from the parent preset based on the build type
        parent_preset_name = "debug"
        bt_lower = self.options.build_type.lower()
        if bt_lower not in ("debug", "debugfast", "debug-fast"):
            parent_preset_name = "release"
            if bt_lower == "relwithdebinfo":
                parent_preset_name = "relwithdebinfo"

        # Nice display name for the preset
        display_name = f"{self.options.build_type}"
        if bt_lower in ("debugfast", "debug-fast"):
            display_name = "Debug (fast)"
        display_name += f" ({OSDisplayName}"
        if self.options.cc:
            display_name += f": {os.path.basename(self.options.cc)}"
        display_name += ")"

        if self.options.sanitizer:
            display_name += f" with {self.options.sanitizer}"

        generator = "Unix Makefiles" if not self.is_windows() else "Visual Studio 17 2022"
        if self.options.ninja_path:
            generator = "Ninja"
        elif "CMAKE_GENERATOR" in self.compiler_info:
            generator = self.compiler_info["CMAKE_GENERATOR"]

        main_cmake_build_type = "Debug" if self.options.build_type.lower() in ("debugfast", "debug-fast") else self.options.build_type
        cache_vars = {
            "CMAKE_BUILD_TYPE": main_cmake_build_type,
            "MRDOCS_BUILD_DOCS": False,
            "MRDOCS_GENERATE_REFERENCE": False,
            "MRDOCS_GENERATE_ANTORA_REFERENCE": False
        }
        # Rebuild package roots strictly from recipe metadata for this run
        self.package_roots = {}
        for rec in self.recipe_info.values():
            if rec.package_root_var:
                self.package_roots[rec.package_root_var] = rec.install_dir

        # Only set explicit *_ROOT cache variables; avoid CMAKE_PREFIX_PATH to prevent regex issues
        dedup_roots: Dict[str, Tuple[str, str]] = {}
        for var, path in self.package_roots.items():
            k = var.lower()
            # Prefer lowercase variable name if both forms exist
            if k not in dedup_roots or var.islower():
                dedup_roots[k] = (var, path)
        # rewrite package_roots to the deduped, preferred casing
        self.package_roots = {var: path for (_, (var, path)) in dedup_roots.items()}
        for var, path in self.package_roots.items():
            cache_vars[var] = path
        # Ensure no stray prefix path sneaks in
        cache_vars.pop("CMAKE_PREFIX_PATH", None)

        new_preset = {
            "name": self.options.preset,
            "generator": generator,
            "displayName": display_name,
            "description": f"Preset for building MrDocs in {self.options.build_type} mode with the {os.path.basename(self.options.cc) if self.options.cc else 'default'} compiler in {OSDisplayName}.",
            "inherits": parent_preset_name,
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": cache_vars,
            "warnings": {
                "unusedCli": False
            },
            "condition": {
                "type": "equals",
                "lhs": "${hostSystemName}",
                "rhs": hostSystemName
            }
        }

        if generator.startswith("Visual Studio"):
            new_preset["architecture"] = "x64"

        if self.options.cc:
            new_preset["cacheVariables"]["CMAKE_C_COMPILER"] = self.options.cc
        if self.options.cxx:
            new_preset["cacheVariables"]["CMAKE_CXX_COMPILER"] = self.options.cxx
        if self.options.ninja_path:
            new_preset["cacheVariables"]["CMAKE_MAKE_PROGRAM"] = self.options.ninja_path
            new_preset["generator"] = "Ninja"

        cc_flags = ''
        cxx_flags = ''
        if self.options.sanitizer:
            flag_name = self.sanitizer_flag_name(self.options.sanitizer)
            cc_flags = f"-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer"
            cxx_flags = f"-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer"

        cache_config_args: List[str] = []
        cc_flags, cxx_flags = self._inject_clang_toolchain_flags(cache_config_args, cc_flags, cxx_flags)
        for arg in cache_config_args:
            key, value = arg.split("=", 1)
            key = key.replace("-D", "", 1)
            new_preset["cacheVariables"][key] = value
        if cc_flags:
            new_preset["cacheVariables"]['CMAKE_C_FLAGS'] = cc_flags.strip()
        if cxx_flags:
            new_preset["cacheVariables"]['CMAKE_CXX_FLAGS'] = cxx_flags.strip()

        # if build type is debug and compiler is clang (default macos or explicitly clang),
        # add "CMAKE_CXX_FLAGS": "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE"
        # or append it to existing CMAKE_CXX_FLAGS
        if self.options.build_type.lower() == "debug":
            is_clang = False
            if self.options.cxx and "clang" in os.path.basename(self.options.cxx).lower():
                is_clang = True
            elif "CMAKE_CXX_COMPILER_ID" in self.compiler_info and self.compiler_info["CMAKE_CXX_COMPILER_ID"].lower() == "clang":
                is_clang = True
            if is_clang:
                hardening_flag = "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE"
                if "CMAKE_CXX_FLAGS" in new_preset["cacheVariables"]:
                    new_preset["cacheVariables"]["CMAKE_CXX_FLAGS"] += " " + hardening_flag
                else:
                    new_preset["cacheVariables"]["CMAKE_CXX_FLAGS"] = hardening_flag

        if self.is_windows():
            if self.options.python_path:
                new_preset["cacheVariables"]["PYTHON_EXECUTABLE"] = self.options.python_path
            if self.options.git_path:
                new_preset["cacheVariables"]["GIT_EXECUTABLE"] = self.options.git_path
                new_preset["cacheVariables"]["GIT_ROOT"] = os.path.dirname(self.options.git_path)

        # Add vendor information for Visual Studio settings if on Windows
        if self.is_windows():
            new_preset["vendor"] = {
                "microsoft.com/VisualStudioSettings/CMake/1.0": {
                    "hostOS": ["Windows"],
                    "intelliSenseMode": "windows-msvc-x64"
                }
            }

        # Update cache variables path prefixes with their relative equivalents (semicolon-safe)
        source_dir_parent = os.path.dirname(self.options.source_dir)
        if source_dir_parent == self.options.source_dir:
            source_dir_parent = ''
        home_dir = os.path.expanduser("~")

        def normalize_value(val: str) -> str:
            if not isinstance(val, str):
                return val
            parts = val.split(";")
            out_parts = []
            for part in parts:
                p = part
                if self.options.source_dir and p.startswith(self.options.source_dir):
                    p = "${sourceDir}" + p[len(self.options.source_dir):]
                elif source_dir_parent and p.startswith(source_dir_parent):
                    p = "${sourceParentDir}" + p[len(source_dir_parent):]
                elif home_dir and p.startswith(home_dir):
                    p = "$env{HOME}" + p[len(home_dir):]
                out_parts.append(p)
            return ";".join(out_parts)

        for key, value in list(new_preset["cacheVariables"].items()):
            if isinstance(value, str):
                new_preset["cacheVariables"][key] = normalize_value(value)

        # Upsert preset
        preset_exists = False
        for preset in user_presets.get("configurePresets", []):
            if preset.get("name") == self.options.preset:
                preset_exists = True
                # Update the existing preset
                preset.update(new_preset)
                break
        if not preset_exists:
            # Add the new preset to the list
            user_presets.setdefault("configurePresets", []).append(new_preset)

        # Write the updated presets back to the file
        self.write_text(user_presets_path, json.dumps(user_presets, indent=4))

    def _git_symlink_entries(self, repo_dir):
        """
        Returns a list of (worktree_path, intended_target_string) for all git-tracked symlinks (mode 120000).
        """
        out = subprocess.check_output(
            [self.options.git_path, "-C", repo_dir, "ls-files", "-s"],
            text=True, encoding="utf-8", errors="replace"
        )
        entries = []
        for line in out.splitlines():
            # "<mode> <object> <stage>\t<path>"
            # Example for symlink: "120000 e69de29... 0\tpath/to/link"
            try:
                head, path = line.split("\t", 1)
                mode, obj, _stage = head.split()[:3]
            except ValueError:
                continue
            if mode != "120000":
                continue
            target = subprocess.check_output(
                [self.options.git_path, "-C", repo_dir, "cat-file", "-p", obj],
                text=True, encoding="utf-8", errors="replace"
            ).rstrip("\n")
            entries.append((path, target))
        return entries

    def _same_link_target(self, link_path, intended):
        """Return True if link_path is a symlink pointing to intended (normalized)."""
        try:
            current = os.readlink(link_path)
        except OSError:
            return False

        def norm(p):
            return os.path.normpath(p.replace("/", os.sep))

        return norm(current) == norm(intended)

    def _make_symlink_or_fallback(self, file_path, intended_target, repo_dir):
        """
        Create a symlink at file_path pointing to intended_target (POSIX path from git).
        Falls back to hardlink/copy on Windows if symlinks aren’t permitted.
        Returns: 'symlink' | 'hardlink' | 'copy'
        """
        if self.options.dry_run:
            self.ui.info(f"dry-run: would ensure symlink {file_path} -> {intended_target}")
            return "dry-run"

        parent = os.path.dirname(file_path)
        if parent and not os.path.isdir(parent):
            self.ensure_dir(parent)

        # Remove existing non-symlink file
        if os.path.exists(file_path) and not os.path.islink(file_path):
            if self.options.dry_run:
                self.ui.info(f"dry-run: would remove file {file_path}")
            else:
                os.remove(file_path)

        # Git stores POSIX-style link text; translate to native separators for the OS call
        native_target = intended_target.replace("/", os.sep)

        # Detect if the final target (as it would resolve in the WT) is a directory (Windows needs this)
        resolved_target = os.path.normpath(os.path.join(parent, native_target))
        target_is_dir = os.path.isdir(resolved_target)

        # Try real symlink first
        try:
            # On Windows, target_is_directory must be correct for directory links
            if os.name == "nt":
                if self.options.dry_run:
                    self.ui.info(f"dry-run: would create symlink {file_path} -> {native_target}")
                else:
                    os.symlink(native_target, file_path, target_is_directory=target_is_dir)
            else:
                if self.options.dry_run:
                    self.ui.info(f"dry-run: would create symlink {file_path} -> {native_target}")
                else:
                    os.symlink(native_target, file_path)
            return "symlink"
        except (NotImplementedError, OSError, PermissionError):
            pass

        # Fallback: hardlink (files only, same volume)
        try:
            if os.path.isfile(resolved_target):
                if self.options.dry_run:
                    self.ui.info(f"dry-run: would create hardlink {file_path} -> {resolved_target}")
                else:
                    os.link(resolved_target, file_path)
                return "hardlink"
        except OSError:
            pass

        # Last resort: copy the file contents if it exists
        if os.path.isfile(resolved_target):
            if self.options.dry_run:
                self.ui.info(f"dry-run: would copy {resolved_target} -> {file_path}")
            else:
                shutil.copyfile(resolved_target, file_path)
            return "copy"

        # If the target doesn’t exist in WT, write the intended link text so state is explicit
        self.write_text(file_path, intended_target, encoding="utf-8")
        return "copy"

    def _is_git_repo(self, repo_dir):
        """Return True if repo_dir looks like a Git work tree."""
        if os.path.isdir(os.path.join(repo_dir, ".git")):
            return True
        try:
            out = subprocess.check_output(
                [self.options.git_path, "-C", repo_dir, "rev-parse", "--is-inside-work-tree"],
                stderr=subprocess.DEVNULL, text=True
            )
            return out.strip() == "true"
        except Exception:
            return False

    def check_git_symlinks(self, repo_dir):
        """
        Ensure all Git-tracked symlinks in repo_dir are correct in the working tree.
        Fixes text-file placeholders produced when core.symlinks=false.
        """
        repo_dir = os.path.abspath(repo_dir)
        if not self._is_git_repo(repo_dir):
            return

        symlinks = self._git_symlink_entries(repo_dir)
        if not symlinks:
            return

        fixed = {"symlink": 0, "hardlink": 0, "copy": 0, "already_ok": 0}

        for rel_path, intended in symlinks:
            link_path = os.path.join(repo_dir, rel_path)

            # Already OK?
            if os.path.islink(link_path) and self._same_link_target(link_path, intended):
                fixed["already_ok"] += 1
                continue

            # If it's a regular file that merely contains the target text, replace it anyway
            if os.path.exists(link_path) and not os.path.islink(link_path):
                try:
                    with open(link_path, "r", encoding="utf-8") as f:
                        content = f.read().strip()
                    # no-op: we still replace below if content == intended (or even if not)
                except Exception:
                    # unreadable is fine; we’ll still replace
                    pass

            kind = self._make_symlink_or_fallback(link_path, intended, repo_dir)
            fixed[kind] += 1

        # Summary + Windows hint
        if (fixed["symlink"] + fixed["hardlink"] + fixed["copy"]) > 0:
            print(
                f"Repaired Git symlinks in {repo_dir} "
                f"(created: {fixed['symlink']} symlink(s), {fixed['hardlink']} hardlink(s), "
                f"{fixed['copy']} copy/copies; {fixed['already_ok']} already OK)."
            )
            if fixed["hardlink"] or fixed["copy"]:
                print(
                    "Warning: Some symlinks could not be created. On Windows, enable Developer Mode "
                    "or run with privileges that allow creating symlinks. Also ensure "
                    "`git config core.symlinks true` before checkout."
                )

    def install_mrdocs(self):
        self.check_git_symlinks(self.options.source_dir)

        # build_dir/install_dir already collected; ensure they are set relative to preset if empty
        if not self.options.build_dir:
            self.options.build_dir = os.path.join(self.options.source_dir, "build", self.options.preset)
        if not self.options.system_install and not self.options.install_dir:
            self.options.install_dir = os.path.join(self.options.source_dir, "install", self.options.preset)

        extra_args = []
        if not self.options.system_install and self.options.install_dir:
            extra_args.extend(["-D", f"CMAKE_INSTALL_PREFIX={self.options.install_dir}"])

        extra_args.append(f"--preset={self.options.preset}")

        main_build_type = "Debug" if self.options.build_type.lower() in ("debugfast", "debug-fast") else self.options.build_type
        self.cmake_workflow(self.options.source_dir, main_build_type, self.options.build_dir,
                            self.options.install_dir, extra_args, force_rebuild=False,
                            remove_build_dir=False, allow_skip=False)

        if self.options.build_dir and self.prompt_option("run_tests", "Run tests after build"):
            # Look for ctest path relative to the cmake path
            ctest_path = os.path.join(os.path.dirname(self.options.cmake_path), "ctest")
            if self.is_windows():
                ctest_path += ".exe"
            if not os.path.exists(ctest_path):
                raise FileNotFoundError(
                    f"ctest executable not found at {ctest_path}. Please ensure CMake is installed correctly.")
            test_args = [ctest_path, "--test-dir", self.options.build_dir, "--output-on-failure", "--progress",
                         "--no-tests=error", "--output-on-failure", "--parallel", str(os.cpu_count() or 1)]
            self.run_cmd(test_args)

        self.ui.ok(f"MrDocs has been successfully installed in {self.options.install_dir}.")

    @lru_cache(maxsize=1)
    def libxml2_root_dir(self):
        for key, path in self.package_roots.items():
            if "libxml2" in key.lower():
                return path
        return None

    def generate_clion_run_configs(self, configs):
        import xml.etree.ElementTree as ET

        # Generate CLion run configurations for MrDocs
        # For each configuration, we create an XML file in <source-dir>/.run
        # named <config-name>.run.xml
        run_dir = os.path.join(self.options.source_dir, ".run")
        self.ensure_dir(run_dir)
        for config in configs:
            config_name = config["name"]
            run_config_path = os.path.join(run_dir, f"{config_name}.run.xml")
            root = ET.Element("component", name="ProjectRunConfigurationManager")
            if 'target' in config:
                attrib = {
                    "default": "false",
                    "name": config["name"],
                    "type": "CMakeRunConfiguration",
                    "factoryName": "Application",
                    "PROGRAM_PARAMS": ' '.join(shlex.quote(arg) for arg in config["args"]),
                    "REDIRECT_INPUT": "false",
                    "ELEVATE": "false",
                    "USE_EXTERNAL_CONSOLE": "false",
                    "EMULATE_TERMINAL": "false",
                    "PASS_PARENT_ENVS_2": "true",
                    "PROJECT_NAME": "MrDocs",
                    "TARGET_NAME": config["target"],
                    "CONFIG_NAME": self.options.preset or "debug",
                    "RUN_TARGET_PROJECT_NAME": "MrDocs",
                    "RUN_TARGET_NAME": config["target"]
                }
                if 'folder' in config:
                    attrib["folderName"] = config["folder"]
                clion_config = ET.SubElement(root, "configuration", attrib)
                if 'env' in config:
                    envs = ET.SubElement(clion_config, "envs")
                    for key, value in config['env'].items():
                        ET.SubElement(envs, "env", name=key, value=value)
                method = ET.SubElement(clion_config, "method", v="2")
                ET.SubElement(method, "option",
                              name="com.jetbrains.cidr.execution.CidrBuildBeforeRunTaskProvider$BuildBeforeRunTask",
                              enabled="true")
            elif 'script' in config:
                if config["script"].endswith(".py"):
                    attrib = {
                        "default": "false",
                        "name": config["name"],
                        "type": "PythonConfigurationType",
                        "factoryName": "Python",
                        "nameIsGenerated": "false"
                    }
                    if 'folder' in config:
                        attrib["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrib)
                    ET.SubElement(clion_config, "module", name="mrdocs")
                    ET.SubElement(clion_config, "option", name="ENV_FILES", value="")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="PARENT_ENVS", value="true")
                    envs = ET.SubElement(clion_config, "envs")
                    ET.SubElement(envs, "env", name="PYTHONUNBUFFERED", value="1")
                    ET.SubElement(clion_config, "option", name="SDK_HOME", value="")
                    if 'cwd' in config and config["cwd"] != self.options.source_dir:
                        ET.SubElement(clion_config, "option", name="WORKING_DIRECTORY", value=config["cwd"])
                    else:
                        ET.SubElement(clion_config, "option", name="WORKING_DIRECTORY", value="$PROJECT_DIR$")
                    ET.SubElement(clion_config, "option", name="IS_MODULE_SDK", value="true")
                    ET.SubElement(clion_config, "option", name="ADD_CONTENT_ROOTS", value="true")
                    ET.SubElement(clion_config, "option", name="ADD_SOURCE_ROOTS", value="true")
                    ET.SubElement(clion_config, "option", name="SCRIPT_NAME", value=config["script"])
                    ET.SubElement(clion_config, "option", name="PARAMETERS",
                                  value=' '.join(shlex.quote(arg) for arg in config["args"]))
                    ET.SubElement(clion_config, "option", name="SHOW_COMMAND_LINE", value="false")
                    ET.SubElement(clion_config, "option", name="EMULATE_TERMINAL", value="false")
                    ET.SubElement(clion_config, "option", name="MODULE_MODE", value="false")
                    ET.SubElement(clion_config, "option", name="REDIRECT_INPUT", value="false")
                    ET.SubElement(clion_config, "option", name="INPUT_FILE", value="")
                    ET.SubElement(clion_config, "method", v="2")
                elif config["script"].endswith(".sh"):
                    attrib = {
                        "default": "false",
                        "name": config["name"],
                        "type": "ShConfigurationType"
                    }
                    if 'folder' in config:
                        attrib["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrib)
                    ET.SubElement(clion_config, "option", name="SCRIPT_TEXT",
                                  value=f"bash {shlex.quote(config['script'])}")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_PATH", value="true")
                    ET.SubElement(clion_config, "option", name="SCRIPT_PATH", value=config["script"])
                    ET.SubElement(clion_config, "option", name="SCRIPT_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_WORKING_DIRECTORY", value="true")
                    if 'cwd' in config and config["cwd"] != self.options.source_dir:
                        ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value=config["cwd"])
                    else:
                        ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value="$PROJECT_DIR$")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_INTERPRETER_PATH", value="true")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_PATH", value="")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="EXECUTE_IN_TERMINAL", value="true")
                    ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="false")
                    ET.SubElement(clion_config, "envs")
                    ET.SubElement(clion_config, "method", v="2")
                elif config["script"].endswith(".js"):
                    attrb = {
                        "default": "false",
                        "name": config["name"],
                        "type": "NodeJSConfigurationType",
                        "path-to-js-file": config["script"],
                        "working-dir": config.get("cwd", "$PROJECT_DIR$")
                    }
                    if 'folder' in config:
                        attrb["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrb)
                    envs = ET.SubElement(clion_config, "envs")
                    if 'env' in config:
                        for key, value in config['env'].items():
                            ET.SubElement(envs, "env", name=key, value=value)
                    ET.SubElement(clion_config, "method", v="2")
                elif config["script"] == "npm":
                    attrib = {
                        "default": "false",
                        "name": config["name"],
                        "type": "js.build_tools.npm"
                    }
                    if 'folder' in config:
                        attrib["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrib)
                    ET.SubElement(clion_config, "package-json", value=os.path.join(config["cwd"], "package.json"))
                    ET.SubElement(clion_config, "command", value=config["args"][0] if config["args"] else "ci")
                    ET.SubElement(clion_config, "node-interpreter", value="project")
                    envs = ET.SubElement(clion_config, "envs")
                    if 'env' in config:
                        for key, value in config['env'].items():
                            ET.SubElement(envs, "env", name=key, value=value)
                    ET.SubElement(clion_config, "method", v="2")
                else:
                    attrib = {
                        "default": "false",
                        "name": config["name"],
                        "type": "ShConfigurationType"
                    }
                    if 'folder' in config:
                        attrib["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrib)
                    args = config.get("args") or []
                    ET.SubElement(clion_config, "option", name="SCRIPT_TEXT",
                                  value=f"{shlex.quote(config['script'])} {' '.join(shlex.quote(arg) for arg in args)}")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_PATH", value="true")
                    ET.SubElement(clion_config, "option", name="SCRIPT_PATH", value=config["script"])
                    ET.SubElement(clion_config, "option", name="SCRIPT_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_WORKING_DIRECTORY", value="true")
                    if 'cwd' in config and config["cwd"] != self.options.source_dir:
                        ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value=config["cwd"])
                    else:
                        ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value="$PROJECT_DIR$")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_INTERPRETER_PATH", value="true")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_PATH", value="")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="EXECUTE_IN_TERMINAL", value="true")
                    ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="false")
                    ET.SubElement(clion_config, "envs")
                    ET.SubElement(clion_config, "method", v="2")

            tree = ET.ElementTree(root)
            if self.options.dry_run:
                self.ui.info(f"dry-run: would write CLion run configuration {run_config_path}")
            else:
                tree.write(run_config_path, encoding="utf-8", xml_declaration=False)

    def generate_visual_studio_run_configs(self, configs):
        # https://learn.microsoft.com/en-us/visualstudio/ide/customize-build-and-debug-tasks-in-visual-studio?view=vs-2022
        # https://learn.microsoft.com/en-us/cpp/build/launch-vs-schema-reference-cpp?view=msvc-170
        # https://learn.microsoft.com/en-us/cpp/build/tasks-vs-json-schema-reference-cpp?view=msvc-170
        # Visual Studio launch configs are stored in .vs/launch.vs.json
        vs_dir = os.path.join(self.options.source_dir, ".vs")
        self.ensure_dir(vs_dir)
        launch_path = os.path.join(vs_dir, "launch.vs.json")
        tasks_path = os.path.join(vs_dir, "tasks.vs.json")

        # Load existing configs if present
        if os.path.exists(launch_path):
            with open(launch_path, "r") as f:
                launch_data = json.load(f)
        else:
            launch_data = {"version": "0.2.1", "defaults": {}, "configurations": []}

        if os.path.exists(tasks_path):
            with open(tasks_path, "r") as f:
                tasks_data = json.load(f)
        else:
            tasks_data = {"version": "0.2.1", "tasks": []}

        # Build a dict for quick lookup by name
        vs_configs_by_name = {cfg.get("name"): cfg for cfg in launch_data.get("configurations", [])}
        vs_tasks_by_name = {task.get("label"): task for task in tasks_data.get("taskLabel", [])}

        def vs_config_type(config):
            if "script" in config:
                if config["script"].endswith(".py"):
                    return "python"
                elif config["script"].endswith(".js"):
                    return "nodejs"
                else:
                    return "shell"
            elif "target" in config:
                return "default"

        def rel_to_mrdocs_dir(script_path):
            is_subdir_of_source_dir = script_path.replace('\\', '/').rstrip('/').startswith(
                self.options.source_dir.replace('\\', '/').rstrip('/'))
            if is_subdir_of_source_dir:
                return os.path.relpath(script_path, self.options.source_dir)
            return script_path

        def vs_config_project(config):
            if "target" in config:
                return "CMakeLists.txt"
            elif "script" in config:
                return rel_to_mrdocs_dir(config["script"])
            return None

        def vs_config_project_target(config):
            if "target" in config:
                return config["target"] + ".exe"
            return ""

        for config in configs:
            is_python_script = 'script' in config and config['script'].endswith('.py')
            is_config = 'target' in config or is_python_script
            if is_config:
                new_cfg = {
                    "name": config["name"],
                    "type": vs_config_type(config),
                    "project": vs_config_project(config),
                    "projectTarget": vs_config_project_target(config)
                }

                if "cwd" in config:
                    new_cfg["cwd"] = config["cwd"]
                if "env" in config:
                    new_cfg["env"] = config["env"]

                if 'target' in config:
                    if "args" in config:
                        new_cfg["args"] = config["args"]
                if 'script' in config:
                    new_cfg["interpreter"] = "(default)"
                    new_cfg["interpreterArguments"] = ''
                    if "args" in config and isinstance(config["args"], list):
                        new_cfg["scriptArguments"] = subprocess.list2cmdline(config["args"])
                    else:
                        new_cfg["scriptArguments"] = ""
                    new_cfg["nativeDebug"] = False
                    new_cfg["webBrowserUrl"] = ""

                # Replace or add
                vs_configs_by_name[new_cfg["name"]] = new_cfg
            else:
                new_task = {
                    "taskLabel": config["name"],
                    # appliesTo script meaning we'll see the tasks as an option
                    # when right-clicking on the script in Visual Studio
                    "appliesTo": vs_config_project(config),
                    "type": "launch",
                    "command": config.get("script", ""),
                    "args": config.get("args", []),
                }

                if 'env' in config:
                    new_task["env"] = config["env"]

                if 'cwd' in config:
                    new_task["workingDirectory"] = config["cwd"]

                if new_task["command"].endswith(".js"):
                    new_task["args"] = [new_task["command"]] + new_task["args"]
                    new_task["command"] = "node"
                elif new_task["command"] == "npm" and "workingDirectory" in new_task:
                    new_task["appliesTo"] = os.path.join(new_task["workingDirectory"], "package.json")
                    new_task["appliesTo"] = rel_to_mrdocs_dir(new_task["appliesTo"])
                elif new_task["taskLabel"] == "MrDocs Generate RelaxNG Schema":
                    new_task["appliesTo"] = "mrdocs.rnc"
                elif new_task["taskLabel"] == "MrDocs XML Lint with RelaxNG Schema":
                    new_task["appliesTo"] = "mrdocs.rng"

                vs_tasks_by_name[new_task["taskLabel"]] = new_task

        # Write back all configs
        launch_data["configurations"] = list(vs_configs_by_name.values())
        self.write_text(launch_path, json.dumps(launch_data, indent=4))

        tasks_data["tasks"] = list(vs_tasks_by_name.values())
        self.write_text(tasks_path, json.dumps(tasks_data, indent=4))

    def generate_vscode_run_configs(self, configs):
        if not self.prompt_option("generate_run_configs", "Generate run configs"):
            return

        # Visual Studio launch configs are stored in .vs/launch.vs.json
        vscode_dir = os.path.join(self.options.source_dir, ".vscode")
        self.ensure_dir(vscode_dir)
        launch_path = os.path.join(vscode_dir, "launch.json")
        tasks_path = os.path.join(vscode_dir, "tasks.json")

        # Load existing configs if present
        if os.path.exists(launch_path):
            with open(launch_path, "r") as f:
                launch_data = json.load(f)
        else:
            launch_data = {"version": "0.2.0", "configurations": []}

        if os.path.exists(tasks_path):
            with open(tasks_path, "r") as f:
                tasks_data = json.load(f)
        else:
            tasks_data = {"version": "2.0.0", "tasks": []}

        # Build a dict for quick lookup by name
        vs_configs_by_name = {cfg.get("name"): cfg for cfg in launch_data.get("configurations", [])}
        vs_tasks_by_name = {task.get("label"): task for task in tasks_data.get("tasks", [])}

        # Replace with config placeholders
        def replace_with_placeholders(new_config):
            for key, value in new_config.items():
                if isinstance(value, str):
                    new_config[key] = value.replace(self.options.source_dir, "${workspaceFolder}")
                elif isinstance(value, list):
                    for i in range(len(value)):
                        if isinstance(value[i], str):
                            value[i] = value[i].replace(self.options.source_dir, "${workspaceFolder}")
                elif isinstance(value, dict):
                    for sub_key, sub_value in value.items():
                        if isinstance(sub_value, str):
                            value[sub_key] = sub_value.replace(self.options.source_dir, "${workspaceFolder}")

        bootstrap_refresh_config_name = self.options.preset or self.options.build_type or "debug"
        for config in configs:
            is_python_script = 'script' in config and config['script'].endswith('.py')
            is_js_script = 'script' in config and config['script'].endswith('.js')
            is_config = 'target' in config or is_python_script or is_js_script
            if is_config:
                new_cfg = {
                    "name": config["name"],
                    "type": None,
                    "request": "launch",
                    "program": config.get("script", "") or config.get("target", ""),
                    "args": config["args"].copy(),
                    "cwd": config.get('cwd', self.options.build_dir)
                }

                if 'target' in config:
                    # new_cfg["projectTarget"] = config["target"]
                    new_cfg["name"] += f" ({bootstrap_refresh_config_name})"
                    new_cfg["type"] = "cppdbg"
                    if 'program' in config:
                        new_cfg["program"] = config["program"]
                    else:
                        new_cfg["program"] = os.path.join(self.options.build_dir, config["target"])
                    new_cfg["environment"] = []
                    new_cfg["stopAtEntry"] = False
                    new_cfg["externalConsole"] = False
                    new_cfg["preLaunchTask"] = f"CMake Build {config['target']} ({bootstrap_refresh_config_name})"
                    if self.compiler_info["CMAKE_CXX_COMPILER_ID"].lower() != "clang":
                        lldb_path = shutil.which("lldb")
                        if lldb_path:
                            new_cfg["MIMode"] = "lldb"
                        else:
                            clang_path = self.compiler_info["CMAKE_CXX_COMPILER"]
                            if clang_path and os.path.exists(clang_path):
                                lldb_path = os.path.join(os.path.dirname(clang_path), "lldb")
                                if os.path.exists(lldb_path):
                                    new_cfg["MIMode"] = "lldb"
                    elif self.compiler_info["CMAKE_CXX_COMPILER_ID"].lower() == "gcc":
                        gdb_path = shutil.which("gdb")
                        if gdb_path:
                            new_cfg["MIMode"] = "gdb"
                        else:
                            gcc_path = self.compiler_info["CMAKE_CXX_COMPILER"]
                            if gcc_path and os.path.exists(gcc_path):
                                gdb_path = os.path.join(os.path.dirname(gcc_path), "gdb")
                                if os.path.exists(gdb_path):
                                    new_cfg["MIMode"] = "gdb"
                if 'script' in config:
                    new_cfg["program"] = config["script"]
                    # set type
                    if config["script"].endswith(".py"):
                        new_cfg["type"] = "debugpy"
                        new_cfg["console"] = "integratedTerminal"
                        new_cfg["stopOnEntry"] = False
                        new_cfg["justMyCode"] = True
                        new_cfg["env"] = {}
                    elif config["script"].endswith(".js"):
                        new_cfg["type"] = "node"
                        new_cfg["console"] = "integratedTerminal"
                        new_cfg["internalConsoleOptions"] = "neverOpen"
                        new_cfg["skipFiles"] = [
                            "<node_internals>/**"
                        ]
                        new_cfg["sourceMaps"] = True
                        new_cfg["env"] = {}
                        for key, value in config.get("env", {}).items():
                            new_cfg["env"][key] = value
                    else:
                        raise ValueError(
                            f"Unsupported script type for configuration '{config['name']}': {config['script']}. "
                            "Only Python (.py) and JavaScript (.js) scripts are supported."
                        )

                # Any property that begins with the value of source_dir is replaced with ${workspaceFolder}
                replace_with_placeholders(new_cfg)

                # Replace or add
                vs_configs_by_name[new_cfg["name"]] = new_cfg
            else:
                def to_task_args(config):
                    if 'args' in config:
                        if isinstance(config['args'], list):
                            return config['args'].copy()
                    return []

                # This is a script configuration, we will create a task for it
                new_task = {
                    "label": config["name"],
                    "type": "shell",
                    "command": config["script"],
                    "args": to_task_args(config),
                    "options": {},
                    "problemMatcher": [],
                }
                if 'cwd' in config and config["cwd"] != self.options.source_dir:
                    new_task["options"]["cwd"] = config["cwd"]

                # Any property that begins with the value of source_dir is replaced with ${workspaceFolder}
                replace_with_placeholders(new_task)

                # Replace or add
                vs_tasks_by_name[new_task["label"]] = new_task

        # Create tasks for the cmake config and build steps
        cmake_config_args = [
            "-S", "${workspaceFolder}"
        ]
        if self.options.preset:
            cmake_config_args.extend(["--preset", self.options.preset])
        else:
            cmake_config_args.extend(["-B", self.options.build_dir])
            if self.options.ninja_path:
                cmake_config_args.extend(["-G", "Ninja"])
        cmake_config_task = {
            "label": f"CMake Configure ({bootstrap_refresh_config_name})",
            "type": "shell",
            "command": "cmake",
            "args": cmake_config_args,
            "options": {
                "cwd": "${workspaceFolder}"
            }
        }
        replace_with_placeholders(cmake_config_task)
        vs_tasks_by_name[cmake_config_task["label"]] = cmake_config_task

        unique_targets = set()
        for config in configs:
            if 'target' in config:
                unique_targets.add(config['target'])
        for target in unique_targets:
            build_args = [
                "--build", self.options.build_dir,
                "--target", target
            ]
            cmake_build_task = {
                "label": f"CMake Build {target} ({bootstrap_refresh_config_name})",
                "type": "shell",
                "command": "cmake",
                "args": build_args,
                "options": {
                    "cwd": "${workspaceFolder}"
                },
                "dependsOn": f"CMake Configure ({bootstrap_refresh_config_name})",
                "dependsOrder": "sequence",
                "group": "build"
            }
            replace_with_placeholders(cmake_build_task)
            vs_tasks_by_name[cmake_build_task["label"]] = cmake_build_task

        # Write back all configs
        launch_data["configurations"] = list(vs_configs_by_name.values())
        self.write_text(launch_path, json.dumps(launch_data, indent=4))

        tasks_data["tasks"] = list(vs_tasks_by_name.values())
        self.write_text(tasks_path, json.dumps(tasks_data, indent=4))

    def generate_run_configs(self):
        if self.options.dry_run:
            self.ui.info("dry-run: skipping IDE run configuration generation")
            return

        var_pattern = re.compile(r"\$(\w+)|\${([^}]+)}")

        def expand_with(s: str, mapping: Dict[str, Any]) -> str:
            def repl(m):
                key = m.group(1) or m.group(2)
                return str(mapping.get(key, m.group(0)))
            return var_pattern.sub(repl, s)

        def format_values(obj, tokens):
            if isinstance(obj, str):
                return expand_with(obj, tokens)
            if isinstance(obj, list):
                return [format_values(x, tokens) for x in obj]
            if isinstance(obj, dict):
                return {k: format_values(v, tokens) for k, v in obj.items()}
            return obj

        defaults_path = os.path.join(self.options.source_dir, "share", "run_configs.json")
        defaults = self._load_json_file(defaults_path) or {}

        configs: List[Dict[str, Any]] = defaults.get("configs", [])

        if not configs:
            raise RuntimeError("No run configurations found in share/run_configs.json; add configs to proceed.")

        tokens = {
            "build_dir": self.options.build_dir,
            "source_dir": self.options.source_dir,
            "install_dir": self.options.install_dir,
            "docs_script_ext": "bat" if self.is_windows() else "sh",
            "num_cores": os.cpu_count() or 1,
        }
        configs = [format_values(cfg, tokens) for cfg in configs]
        filtered = []
        for cfg in configs:
            req = cfg.get("requires", [])
            include = True
            if "build_tests" in req and not self.options.build_tests:
                include = False
            if "java" in req and not self.options.java_path:
                include = False
            if include:
                cfg.pop("requires", None)
                filtered.append(cfg)
        configs = filtered

        # Append dynamic configs that must be computed (bootstrap helpers, boost docs, schema lint)
        configs.extend(self._dynamic_run_configs())

        target_vscode = bool(defaults.get("vscode", True))
        target_clion = bool(defaults.get("clion", True))
        target_vs = bool(defaults.get("vs", True))

        if target_clion and self.prompt_option("generate_clion_run_configs", "CLion"):
            self.ui.info("Generating CLion run configurations...")
            self.generate_clion_run_configs(configs)
        if target_vscode and self.prompt_option("generate_vscode_run_configs", "VS Code"):
            self.ui.info("Generating Visual Studio Code run configurations...")
            self.generate_vscode_run_configs(configs)
        if target_vs and self.prompt_option("generate_vs_run_configs", "Visual Studio"):
            self.ui.info("Generating Visual Studio run configurations...")
            self.generate_visual_studio_run_configs(configs)

    def _dynamic_run_configs(self) -> List[Dict[str, Any]]:
        configs: List[Dict[str, Any]] = []
        # Bootstrap helper targets
        bootstrap_args: List[str] = []
        for field in dataclasses.fields(InstallOptions):
            value = getattr(self.options, field.name)
            default_value = getattr(self.default_options, field.name, None)
            if value is not None and (value != default_value or field.name == "build_type"):
                if field.name == "non_interactive":
                    continue
                if field.type is bool:
                    if value:
                        bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                    else:
                        bootstrap_args.append(f"--no-{field.name.replace('_', '-')}")
                elif field.type is str:
                    if value != "":
                        bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                        bootstrap_args.append(value)
                else:
                    raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")

        bootstrap_refresh_config_name = self.options.preset or self.options.build_type or "debug"
        configs.extend([
            {"name": "MrDocs Bootstrap Help", "script": os.path.join(self.options.source_dir, "bootstrap.py"), "args": ["--help"], "cwd": self.options.source_dir},
            {"name": f"MrDocs Bootstrap Update ({bootstrap_refresh_config_name})", "script": os.path.join(self.options.source_dir, "bootstrap.py"), "folder": "MrDocs Bootstrap Update", "args": bootstrap_args, "cwd": self.options.source_dir},
            {"name": f"MrDocs Bootstrap Refresh ({bootstrap_refresh_config_name})", "script": os.path.join(self.options.source_dir, "bootstrap.py"), "folder": "MrDocs Bootstrap Refresh", "args": bootstrap_args + ["--non-interactive"], "cwd": self.options.source_dir},
            {"name": "MrDocs Bootstrap Refresh All", "script": os.path.join(self.options.source_dir, "bootstrap.py"), "folder": "MrDocs Bootstrap Refresh", "args": ["--refresh-all"], "cwd": self.options.source_dir},
            {"name": f"MrDocs Generate Config Info ({bootstrap_refresh_config_name})", "script": os.path.join(self.options.source_dir, "util", "generate-config-info.py"), "folder": "MrDocs Generate Config Info", "args": [os.path.join(self.options.source_dir, "src", "lib", "ConfigOptions.json"), os.path.join(self.options.build_dir)], "cwd": self.options.source_dir},
            {"name": "MrDocs Generate Config Info (docs)", "script": os.path.join(self.options.source_dir, "util", "generate-config-info.py"), "folder": "MrDocs Generate Config Info", "args": [os.path.join(self.options.source_dir, "src", "lib", "ConfigOptions.json"), os.path.join(self.options.source_dir, "docs", "config-headers")], "cwd": self.options.source_dir},
            {"name": "MrDocs Generate YAML Schema", "script": os.path.join(self.options.source_dir, "util", "generate-yaml-schema.py"), "args": [], "cwd": self.options.source_dir},
            {"name": "MrDocs Reformat Source Files", "script": os.path.join(self.options.source_dir, "util", "reformat.py"), "args": [], "cwd": self.options.source_dir},
        ])

        # Boost documentation targets (dynamic scan)
        self.prompt_option("boost_src_dir", "Boost source")
        num_cores = os.cpu_count() or 1
        if self.options.boost_src_dir and os.path.exists(self.options.boost_src_dir):
            boost_libs = os.path.join(self.options.boost_src_dir, "libs")
            if os.path.exists(boost_libs):
                for lib in os.listdir(boost_libs):
                    mrdocs_config = os.path.join(boost_libs, lib, "doc", "mrdocs.yml")
                    if os.path.exists(mrdocs_config):
                        configs.append({
                            "name": f"Boost.{lib.title()} Documentation",
                            "target": "mrdocs",
                            "folder": "Boost Documentation",
                            "program": os.path.join(self.options.build_dir, "mrdocs"),
                            "args": [
                                "../CMakeLists.txt",
                                f"--config={mrdocs_config}",
                                f"--output={os.path.join(self.options.boost_src_dir, 'libs', lib, 'doc', 'modules', 'reference', 'pages')}",
                                "--generator=adoc",
                                f"--addons={os.path.join(self.options.source_dir, 'share', 'mrdocs', 'addons')}",
                                f"--libc-includes={os.path.join(self.options.source_dir, 'share', 'mrdocs', 'headers', 'libc-stubs')}",
                                "--tagfile=reference.tag.xml",
                                "--multipage=true",
                                f"--concurrency={num_cores}",
                                "--log-level=debug",
                            ],
                        })

        # XML / RelaxNG tasks requiring Java and libxml2
        if self.options.java_path:
            configs.append({
                "name": "MrDocs Generate RelaxNG Schema",
                "script": self.options.java_path,
                "args": [
                    "-jar",
                    os.path.join(self.options.source_dir, "util", "trang.jar"),
                    os.path.join(self.options.source_dir, "mrdocs.rnc"),
                    os.path.join(self.options.build_dir, "mrdocs.rng"),
                ],
                "cwd": self.options.source_dir,
            })
            libxml2_root = self.libxml2_root_dir()
            if libxml2_root:
                libxml2_xmllint_executable = os.path.join(libxml2_root, "bin", "xmllint")
                xml_sources_dir = os.path.join(self.options.source_dir, "test-files", "golden-tests")
                if self.is_windows():
                    xml_sources = []
                    for root, _, files in os.walk(xml_sources_dir):
                        for file in files:
                            if file.endswith(".xml") and not file.endswith(".bad.xml"):
                                xml_sources.append(os.path.join(root, file))
                    configs.append({
                        "name": "MrDocs XML Lint with RelaxNG Schema",
                        "script": libxml2_xmllint_executable,
                        "args": [
                            "--dropdtd",
                            "--noout",
                            "--relaxng",
                            os.path.join(self.options.build_dir, "mrdocs.rng"),
                            *xml_sources,
                        ],
                        "cwd": self.options.source_dir,
                    })
                else:
                    configs.append({
                        "name": "MrDocs XML Lint with RelaxNG Schema",
                        "script": "find",
                        "args": [
                            xml_sources_dir,
                            "-type",
                            "f",
                            "-name",
                            "*.xml",
                            "!",
                            "-name",
                            "*.bad.xml",
                            "-exec",
                            libxml2_xmllint_executable,
                            "--dropdtd",
                            "--noout",
                            "--relaxng",
                            os.path.join(self.options.build_dir, "mrdocs.rng"),
                            "{}",
                            "+",
                        ],
                        "cwd": self.options.source_dir,
                    })
        return configs

    def generate_pretty_printer_configs(self):
        config_path = os.path.join(self.options.source_dir, "share", "pretty_printers.json")
        overrides = self._load_json_file(config_path) or {}

        if self.options.dry_run:
            if overrides:
                self.ui.info("dry-run: would generate debugger pretty printer configuration from share/pretty_printers.json")
            else:
                self.ui.info("dry-run: skipping debugger pretty printer generation (no config found)")
            return

        if not overrides:
            self.ui.info("No debugger pretty printer configuration found in share/pretty_printers.json; skipping generation.")
            return

        project_label = overrides.get("project", "MrDocs")

        def _resolve_paths(paths):
            resolved = []
            for p in paths:
                resolved.append(p if os.path.isabs(p) else os.path.abspath(os.path.join(self.options.source_dir, p)))
            return resolved

        lldb_scripts = _resolve_paths(overrides.get("lldb", []))
        gdb_scripts = _resolve_paths(overrides.get("gdb", []))

        if not lldb_scripts and not gdb_scripts:
            self.ui.info("No debugger pretty printer scripts listed in local/pretty_printers.json; skipping generation.")
            return

        lldbinit_path = os.path.join(self.options.source_dir, ".lldbinit")
        if lldb_scripts:
            if os.path.exists(lldbinit_path):
                self.ui.info(f"LLDB pretty printer configuration already exists at '{lldbinit_path}', skipping generation.")
            else:
                lldb_lines = [
                    f"# LLDB pretty printers for {project_label}",
                    "# Generated by bootstrap.py",
                    "# Enable LLDB to load this file with: echo 'settings set target.load-cwd-lldbinit true' >> ~/.lldbinit",
                ]
                for script in lldb_scripts:
                    lldb_lines.append(f"command script import {script.replace(os.sep, '/')}")
                self.write_text(lldbinit_path, "\n".join(lldb_lines) + "\n")
                self.ui.ok(f"Generated LLDB pretty printer configuration at '{lldbinit_path}'")
        else:
            self.ui.info("No LLDB pretty printer scripts provided; skipping LLDB configuration.")

        gdbinit_path = os.path.join(self.options.source_dir, ".gdbinit")
        if gdb_scripts:
            if os.path.exists(gdbinit_path):
                self.ui.info(f"GDB pretty printer configuration already exists at '{gdbinit_path}', skipping generation.")
            else:
                gdb_lines = [
                    f"# GDB pretty printers for {project_label}",
                    "# Generated by bootstrap.py",
                    "python",
                    "import sys",
                ]
                for script in gdb_scripts:
                    script_dir = os.path.dirname(script)
                    gdb_lines.append(f"sys.path.insert(0, '{script_dir.replace(os.sep, '/')}')")
                    gdb_lines.extend([
                        "try:",
                        f"    import {Path(script).stem} as _bootstrap_pretty",
                        "    _bootstrap_pretty.register_pretty_printers(gdb)",
                        "except Exception as exc:",
                        "    print('warning: failed to register pretty printers:', exc)",
                    ])
                gdb_lines.append("end")
                self.write_text(gdbinit_path, "\n".join(gdb_lines) + "\n")
                self.ui.ok(f"Generated GDB pretty printer configuration at '{gdbinit_path}'")
        else:
            self.ui.info("No GDB pretty printer scripts provided; skipping GDB configuration.")

    def validate_cli_compatibility(self):
        """
        Smoke-test a handful of legacy/expected CLI invocations to ensure parsing still works.
        """
        parser = _build_arg_parser()
        samples = [
            [],
            ["--no-generate-run-configs"],
            ["--dry-run"],
            ["--skip-build"],
            ["--clean"],
            ["--force"],
            ["--plain-ui"],
            ["--recipe", "sample"],
        ]
        for argv in samples:
            try:
                parser.parse_args(argv)
            except SystemExit as exc:
                raise RuntimeError(f"Legacy CLI invocation {' '.join(argv) or '<empty>'} failed: exit {exc.code}") from exc
            except Exception as exc:
                raise RuntimeError(f"Legacy CLI invocation {' '.join(argv) or '<empty>'} failed: {exc}") from exc
        self.ui.ok("CLI backward compatibility parse check passed.")

    def collect_user_inputs(self):
        """
        Phase 1: ask all questions up front for a two-phase flow.
        This keeps prompts grouped before any work begins.
        """
        self.ui.section("MrDocs Bootstrap", icon="🚀")

        # Seed tool path defaults so prompts (or non-interactive runs) don't get empty values
        for tool in ["git", "cmake", "python", "java", "ninja"]:
            found = self.find_tool(tool)
            if found:
                setattr(self.default_options, f"{tool}_path", found)

        # Toolchain early so later steps don't re-prompt
        self.ui.subsection("Toolchain", icon="🧰")
        self.prompt_option("cc", "C compiler")
        self.prompt_option("cxx", "C++ compiler")
        self.prompt_option("sanitizer", "Sanitizer (asan/ubsan/msan/tsan/none)")
        self.prompt_option("git_path", "git")
        self.prompt_option("cmake_path", "cmake")
        self.prompt_option("python_path", "python")
        self.prompt_option("java_path", "java")
        self.prompt_option("ninja_path", "ninja")

        # Layout / presets
        self.ui.subsection("Source & build", icon="📂")
        self.prompt_option("build_type", "Build type")
        self.prompt_option("preset", "CMake preset")
        self.prompt_option("build_dir", "Build dir")
        self.prompt_option("system_install", "Install to system dirs")
        self.prompt_option("install_dir", "Install dir")
        self.prompt_option("third_party_src_dir", "3rd-party root (src/build/install)")

        # Testing toggles
        self.ui.subsection("Testing", icon="🧪")
        self.prompt_option("build_tests", "Build tests")
        self.prompt_option("run_tests", "Run tests after build")
        self.prompt_option("boost_src_dir", "Boost source")

        # IDE / debugger choices
        self.ui.subsection("Run configs & debuggers", icon="💻")
        self.prompt_option("generate_run_configs", "Generate run configs")
        self.prompt_option("generate_clion_run_configs", "CLion")
        self.prompt_option("generate_vscode_run_configs", "VS Code")
        self.prompt_option("generate_vs_run_configs", "Visual Studio")
        self.prompt_option("generate_pretty_printer_configs", "Pretty printers")

        # Housekeeping toggles
        self.ui.subsection("Maintenance", icon="🧹")
        self.prompt_option("force_rebuild", "Force rebuild deps")
        self.prompt_option("remove_build_dir", "Remove dep build dir")


    def install_all(self):
        # Gather inputs first (two-phase flow)
        self.collect_user_inputs()

        # compute total steps dynamically based on toggles
        total_steps = 4
        if self.options.generate_run_configs:
            total_steps += 1
        if self.options.generate_pretty_printer_configs:
            total_steps += 1
        current_step = 1

        if self.options.list_recipes:
            recipes = self.load_recipe_files()
            if not recipes:
                self.ui.warn(f"No recipes found in {self.recipes_dir}")
            else:
                self.ui.section("Available recipes", icon="📦")
                for r in recipes:
                    tags = f" [{', '.join(r.tags)}]" if r.tags else ""
                    self.ui.info(f"- {r.name}{tags} (version {r.version})")
            return
        if self.options.skip_build:
            self.ui.info("Skip-build requested; build and install steps will be skipped after initial checks.")

        self.ui.section("Toolchain and environment checks", icon="🧰")
        self.ui.info("Checking compilers, environment, and required tools...")
        self.check_compilers()
        self.probe_msvc_dev_env()
        self.check_tools()
        self.ui.subsection("Toolchain summary", icon="🧾")
        toolchain = [
            ("C compiler", self.options.cc or self.compiler_info.get("CMAKE_C_COMPILER", "auto")),
            ("C++ compiler", self.options.cxx or self.compiler_info.get("CMAKE_CXX_COMPILER", "auto")),
            ("git", self.options.git_path),
            ("cmake", self.options.cmake_path),
            ("python", self.options.python_path),
        ]
        self.ui.kv_block(None, toolchain, indent=4)
        self.ui.info("Toolchain ready.")

        current_step += 1
        self.ui.section("Source and build layout", icon="📂")
        self.setup_source_dir()
        self.setup_third_party_dir()
        self.probe_compilers()
        # Ensure preset name is resolved early
        self.prompt_option("build_type", "Build type")
        self.prompt_option("preset", "CMake preset")
        self.ensure_dir(self.options.third_party_src_dir)

        # Summary block
        summary = [
            ("Build type", self.options.build_type),
            ("Preset", self.options.preset),
            ("Build dir", self.ui.shorten_path(self.options.build_dir)),
            ("Install dir", self.ui.shorten_path(self.options.install_dir)),
            ("3rd-party root", self.ui.shorten_path(self.options.third_party_src_dir)),
        ]
        self.ui.subsection("Configuration summary", icon="📋")
        self.ui.kv_block(None, summary, indent=4)

        current_step += 1
        self.ui.section("Third-party dependencies", icon="📦")
        # Ninja is treated like any other dependency now
        self.ui.subsection("ninja", icon="📜")
        self.install_ninja()
        # Recipes bundled into the same section
        recipe_list = self.load_recipe_files()
        if recipe_list:
            for recipe in self._topo_sort_recipes(recipe_list):
                self.ui.subsection(f"{recipe.name}", icon="📜")
                resolved_ref = self.fetch_recipe_source(recipe)
                self.apply_recipe_patches(recipe)
                self.recipe_info[recipe.name] = recipe
                if self.options.skip_build:
                    continue
                if self.is_recipe_up_to_date(recipe, resolved_ref) and not self.options.force:
                    self.ui.ok(f"[{recipe.name}] up to date; skipping build.")
                    continue
                self.build_recipe(recipe)
                self.write_recipe_stamp(recipe, resolved_ref)
                if recipe.package_root_var:
                    self.package_roots[recipe.package_root_var] = recipe.install_dir
        else:
            raise RuntimeError(f"No recipes found in {self.recipes_dir}. Add recipe JSON files to proceed.")

        current_step += 1
        self.ui.section("MrDocs build", icon="⚙️")
        self.ui.subsection("CMake presets")
        self.create_cmake_presets()
        self.show_preset_summary()
        self.ui.subsection("Build and install MrDocs")
        self.install_mrdocs()
        if self.prompt_option("generate_run_configs", "Generate run configs"):
            current_step += 1
        self.ui.section("IDE run configurations", icon="💻")
        self.generate_run_configs()
        if self.prompt_option("generate_pretty_printer_configs", "Pretty printers"):
            current_step += 1
        self.ui.section("Debugger pretty printers", icon="🐞")
        self.generate_pretty_printer_configs()

        # Success footer
        generator = "Ninja" if self.options.ninja_path else self.compiler_info.get("CMAKE_GENERATOR", "unknown")
        footer = [
            ("Preset", self.options.preset),
            ("Build dir", self.ui.shorten_path(self.options.build_dir)),
            ("Install dir", self.ui.shorten_path(self.options.install_dir)),
            ("Generator", generator),
        ]
        self.ui.kv_block("Bootstrap complete", footer, icon="✔️", indent=2)

    def refresh_all(self):
        # 1. Read all configurations in .vscode/launch.json
        current_python_interpreter_path = sys.executable
        this_script_path = os.path.abspath(__file__)
        source_dir = os.path.dirname(this_script_path)
        vscode_launch_path = os.path.join(source_dir, ".vscode", "launch.json")
        vs_launch_path = os.path.join(source_dir, ".vs", "launch.vs.json")
        use_vscode = os.path.exists(vscode_launch_path)
        use_vs = os.path.exists(vs_launch_path)
        if not use_vscode and not use_vs:
            print("No existing Refresh launch configurations found.")
            return
        if use_vscode:
            with open(vscode_launch_path, "r") as f:
                vscode_launch_data = json.load(f)
            configs = vscode_launch_data.get("configurations", [])
        else:
            with open(vs_launch_path, "r") as f:
                vs_launch_data = json.load(f)
            configs = vs_launch_data.get("configurations", [])

        # 2. Filter configurations whose name starts with "MrDocs Bootstrap Refresh ("
        bootstrap_refresh_configs = [
            cfg for cfg in configs if
            cfg.get("name", "").startswith("MrDocs Bootstrap Refresh (") and cfg.get("name", "").endswith(")")
        ]
        if not bootstrap_refresh_configs:
            print("No bootstrap refresh configurations found in Visual Studio Code launch configurations.")
            return

        # 3. For each configuration, run this very same bootstrap.py script with the same arguments
        for config in bootstrap_refresh_configs:
            config_name = config['name']
            if use_vscode:
                args = [arg.replace("${workspaceFolder}", source_dir) for arg in config.get("args", [])]
            else:
                args = shlex.split(config.get("scriptArguments", ""))

            print(f"Refreshing configuration '{config_name}':")
            for arg in args:
                print(f"  * {arg}")
            subprocess.run([current_python_interpreter_path, this_script_path] + args, check=True)


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Bootstrap MrDocs using recipe-driven third-party deps, presets, and IDE/debugger configs.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    skip_cli = {"source_dir"}  # internal-only; not configurable via CLI
    # Preferred flag names for key options (we intentionally drop the old long names).
    custom_flags: Dict[str, List[str]] = {
        "build_type": ["--build-type"],
        "preset": ["--preset"],
        "build_dir": ["--build-dir"],
        "install_dir": ["--install-dir"],
        "third_party_src_dir": ["--third-party-root"],
        "non_interactive": ["-y", "--yes"],
    }

    for field in dataclasses.fields(InstallOptions):
        if field.name in skip_cli:
            continue
        flag_names = custom_flags.get(field.name, [f"--{field.name.replace('_', '-')}"])
        help_text = field.name.replace("_", " ")
        if field.default is not dataclasses.MISSING and field.default is not None:
            if isinstance(field.default, str) and field.default:
                help_text += f" (default: '{field.default}')"
            elif field.default:
                help_text += " (default: true)"
            elif not field.default:
                help_text += " (default: false)"
            else:
                help_text += f" (default: {field.default})"
        if field.type is bool:
            if field.name == "non_interactive":
                parser.add_argument(*flag_names, dest=field.name, action='store_true', help=help_text, default=None)
            else:
                primary = flag_names[0]
                parser.add_argument(*flag_names, dest=field.name, action='store_true', help=help_text, default=None)
                # Provide a no- form for toggling off
                no_flag = primary.replace("--", "--no-", 1) if primary.startswith("--") else f"--no-{field.name}"
                parser.add_argument(no_flag, dest=field.name, action='store_false',
                                    help=f"Set {primary} to false", default=None)
        elif field.type is str:
            parser.add_argument(*flag_names, type=field.type, dest=field.name, help=help_text, default=None)
        else:
            raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")
    return parser


def get_command_line_args(argv=None):
    """
    Parses command line arguments and returns them as a dictionary.

    Every field in the InstallOptions dataclass is converted to a
    valid command line argument description.

    :return: dict: Dictionary of command line arguments.
    """
    parser = _build_arg_parser()
    parsed = vars(parser.parse_args(argv))
    return {k: v for k, v in parsed.items() if v is not None}


def main():
    args = get_command_line_args()
    installer = MrDocsInstaller(args)
    installer.ui.warn(TRANSITION_BANNER)
    if installer.options.refresh_all:
        installer.refresh_all()
        exit(0)
    installer.install_all()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        # Graceful exit when the user hits Ctrl+C/Cmd+C during a prompt
        try:
            ui.ok("🛑 Aborted by user.")
        except Exception:
            print("Aborted by user.")
        sys.exit(130)
