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
Console UI utilities for the bootstrap tool.

Provides the TextUI class for formatted console output with optional
color and emoji support.
"""

import os
import sys
from typing import Optional, List


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
        "warn": "\u26a0\ufe0f  ",  # Two spaces to compensate for terminal rendering issues
        "error": "\u26d4 ",
        "ok": "\u2705 ",
        "section": "",
        "command": "\U0001f4bb ",
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
        self.dry_run: bool = False
        self._ci = bool(os.environ.get("GITHUB_ACTIONS"))
        self._ci_group_open = False
        self._ci_group_title = ""
        self._ci_group_start = 0.0

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
            # Ensure there's a space after non-empty emoji prefix
            if prefix and not prefix.endswith(" "):
                prefix = prefix + " "
        if not self.color_enabled:
            return f"{prefix}{text}"
        color = self.COLOR.get(kind, "")
        reset = self.COLOR["reset"]
        return f"{color}{prefix}{text}{reset}"

    @property
    def _out(self):
        """Output stream for informational messages.

        In dry-run mode, informational messages go to stderr so that
        stdout contains only copy-pasteable shell commands.
        """
        return sys.stderr if self.dry_run else sys.stdout

    def info(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "info", icon), file=self._out)

    def warn(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "warn", icon), file=self._out)

    def error(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "error", icon), file=sys.stderr)

    def error_block(self, header: str, tips: Optional[List[str]] = None):
        print(self._fmt(f"!! {header}", "error"), file=sys.stderr)
        if tips:
            for tip in tips:
                print(self._fmt(f"   \u2022 {tip}", "warn"), file=sys.stderr)

    def ok(self, msg: str, icon: Optional[str] = None):
        print(self._fmt(msg, "ok", icon), file=self._out)

    @property
    def plain(self) -> bool:
        """True when both color and emoji are disabled (CI / --plain mode)."""
        return not self.color_enabled and not self.emoji_enabled

    def section(self, title: str, icon: Optional[str] = None):
        prefix = (icon + " ") if (self.emoji_enabled and icon) else ""
        self.start_group(f"{prefix}{title}")
        line = ("=" if self.plain else "\u2501") * 60
        out = self._out
        print(file=out)
        print(self._fmt(line, "section", ""), file=out)
        print(self._fmt(f"{prefix}{title}", "section", ""), file=out)
        print(self._fmt(line, "section", ""), file=out)

    def command(self, cmd: str, icon: Optional[str] = None):
        print(self._fmt(cmd, "command", icon), file=self._out)

    def subsection(self, title: str, icon: Optional[str] = None):
        prefix = (icon + " ") if (self.emoji_enabled and icon) else ""
        self.start_group(f"{prefix}{title}")
        if self.plain:
            banner = f"--- {prefix}{title}"
        else:
            banner = f"  {prefix}{title}"
        out = self._out
        print(file=out)  # blank line for breathing room
        print(self._fmt(banner, "subsection", ""), file=out)
        if not self.plain:
            # underline matches text length (indent + title) plus a small cushion
            underline_len = max(15, len(banner.strip()) + 4)
            print(self._fmt("-" * underline_len, "subsection", ""), file=out)

    def start_group(self, title: str):
        """Start a CI group. Closes any already-open group first."""
        if self._ci:
            import time
            self.end_group()
            print(f"::group::{title}", file=self._out)
            self._ci_group_open = True
            self._ci_group_title = title
            self._ci_group_start = time.monotonic()

    def end_group(self):
        """Close the current CI group if one is open. No-op otherwise."""
        if self._ci_group_open:
            import time
            elapsed = time.monotonic() - self._ci_group_start
            if elapsed >= 60:
                mins = int(elapsed) // 60
                secs = int(elapsed) % 60
                duration = f"{mins}m {secs}s"
            else:
                duration = f"{elapsed:.1f}s"
            print(f"{self._ci_group_title} completed in {duration}", file=self._out)
            print("::endgroup::", file=self._out)
            self._ci_group_open = False

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
        print(self._fmt(f"{key_fmt}: ", "dim") + self._fmt(display_value, "info"), file=self._out)

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
        out = self._out
        for k, v in items:
            key_fmt = k.rjust(key_width)
            display_value = self.maybe_shorten(v) if isinstance(v, str) else v
            line = f"{pad}{key_fmt}: "
            if self.color_enabled:
                line = f"{self.COLOR['dim']}{line}{self.COLOR['reset']}"
            print(line + self._fmt(str(display_value), "info"), file=out)

    def checklist(self, title: str, items):
        if title:
            self.section(title)
        out = self._out
        for label, done in items:
            if self.plain:
                mark = "[x]" if done else "[ ]"
            else:
                mark = "\u2713" if done else "\u2717"
            style = "ok" if done else "warn"
            print(self._fmt(f"  {mark} {label}", style), file=out)

    def step(self, current: int, total: int, title: str):
        prefix = f"[{current}/{total}] "
        print(self._fmt(f"{prefix}{title}", "subsection"), file=self._out)


# Default UI instance; may be replaced once options are parsed
_default_ui = TextUI()


def get_default_ui() -> TextUI:
    """Get the default UI instance."""
    return _default_ui


def set_default_ui(ui: TextUI):
    """Set the default UI instance."""
    global _default_ui
    _default_ui = ui
