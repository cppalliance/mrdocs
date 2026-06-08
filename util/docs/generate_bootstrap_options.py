#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#
"""Generate the bootstrap options AsciiDoc partial from bootstrap's argparse.

The bootstrap CLI is the source of truth for every option. This script reads
the argument parser defined in `util/bootstrap/src/__main__.py` and writes
the AsciiDoc partial that the install page includes. CI runs `--check` to
fail when the on-disk partial drifts from the parser.

Usage:
    python util/docs/generate_bootstrap_options.py --update
    python util/docs/generate_bootstrap_options.py --check
"""
import argparse
import os
import re
import sys


REPO_ROOT = os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "util/bootstrap"))

from src.__main__ import build_arg_parser  # noqa: E402
from src.core import (  # noqa: E402
    InstallOptions, OPTION_DETAILS, OPTION_DEFAULT_DISPLAY)

# When the dataclass default cannot be reduced to a static, user-facing
# string (it is computed from the environment at install time), describe it
# in prose. Anything not listed here uses the literal value the dataclass
# resolves to when constructed with no overrides.
DYNAMIC_DEFAULTS = {
    "source_dir": "the directory containing the cloned source",
}

PARTIAL_PATH = os.path.join(
    REPO_ROOT,
    "docs/modules/ROOT/partials/bootstrap-options.adoc")

HEADER = (
    "// Auto-generated from util/bootstrap/src/__main__.py.\n"
    "// Run `python util/docs/generate_bootstrap_options.py --update` to "
    "regenerate.\n"
    "// Do not edit by hand; CI checks this file matches the parser.\n"
    "\n"
)


def slugify(group_name):
    return group_name.lower().replace(" ", "-")


def is_flag(action):
    return isinstance(action, (
        argparse._StoreTrueAction, argparse._StoreFalseAction))


def escape_html(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;").replace('"', "&quot;"))


def prose_to_html(text):
    """Render an `OPTION_DETAILS` paragraph as table-cell HTML.

    The table cell is built inside an AsciiDoc passthrough block, which
    means AsciiDoc's inline markup is not applied. Convert backtick-wrapped
    spans (the only AsciiDoc-style markup used in option help) to
    `<code>` so they render as code in the cell rather than literal
    backticks.
    """
    escaped = escape_html(text)
    return re.sub(r"`([^`]+)`", r"<code>\1</code>", escaped)


def anchor_for(action, seen):
    """Stable anchor for an option, derived from its dest or first flag."""
    slug = action.dest or action.option_strings[0].lstrip("-")
    anchor = f"opt-{slugify(slug)}"
    while anchor in seen:
        anchor = anchor + "-alt"
    seen.add(anchor)
    return anchor


def type_label(action):
    """Short type hint shown under the name in the summary table."""
    if is_flag(action):
        return "flag"
    if action.choices:
        return "enum"
    return "string"


def strip_default_suffix(help_text):
    """Drop any trailing "(default: X)" the help text repeats. The default
    column is the source of truth, so keeping the suffix would duplicate it.
    """
    text = (help_text or "").strip()
    return re.sub(r"\s*\(default:\s*[^)]+\)\.?\s*$", "", text).rstrip(" .")


_DEFAULTS_INSTANCE = InstallOptions()


def default_value(action):
    """Resolve the option's default by looking it up on `InstallOptions`.

    Falls back to the argparse `default` attribute when the parser exposes
    a flag whose dest is not present on the dataclass. Returns the raw
    Python value (or a sentinel string for dynamic defaults).
    """
    if action.dest in DYNAMIC_DEFAULTS:
        return DYNAMIC_DEFAULTS[action.dest]
    if hasattr(_DEFAULTS_INSTANCE, action.dest):
        return getattr(_DEFAULTS_INSTANCE, action.dest)
    return action.default


def format_default_html(value, dest=None):
    """Format a default value for the summary-table cell.

    `dest` lets us swap in a human-readable string for defaults whose raw
    value is an internal template (see `OPTION_DEFAULT_DISPLAY`).
    """
    if dest in OPTION_DEFAULT_DISPLAY:
        return prose_to_html(OPTION_DEFAULT_DISPLAY[dest])
    if value is None:
        return ""
    if value is True:
        return "true"
    if value is False:
        return "false"
    text = str(value)
    if text == "":
        return "(none)"
    if value in DYNAMIC_DEFAULTS.values():
        return text
    return f'<code>{escape_html(text)}</code>'


def format_default_adoc(value):
    """Format a default value for the detail bullet list."""
    if value is None:
        return None
    if value is True:
        return "`true`"
    if value is False:
        return "`false`"
    text = str(value)
    if text == "":
        return "(none)"
    if value in DYNAMIC_DEFAULTS.values():
        return text
    escaped = text.replace("`", "\\`")
    return f"`{escaped}`"


def render_description_cell(action):
    """Description cell content: long explanation, optional choices, optional
    meaningful default. Each piece is its own `<p class="tableblock">` so the
    cell reads as a small stack rather than one long run-on paragraph.
    """
    paragraphs = []
    details = OPTION_DETAILS.get(action.dest, "").strip()
    if details:
        paragraphs.append(prose_to_html(details))
    elif action.help:
        # Fallback for any option whose `OPTION_DETAILS` entry has not been
        # written yet (the CI guard prevents this, but rendering should
        # degrade gracefully).
        paragraphs.append(prose_to_html(strip_default_suffix(action.help)))
    if action.choices:
        choices = ", ".join(
            f'<code>{escape_html(str(c))}</code>' for c in action.choices)
        paragraphs.append(f"Choices: {choices}")
    if not is_flag(action):
        default_html = format_default_html(default_value(action), action.dest)
        if default_html and default_html != "(none)":
            paragraphs.append(f"Default: {default_html}")
    return "".join(
        f'<p class="tableblock">{p}</p>' for p in paragraphs)


def render_row(action):
    """One row of an argument-group table."""
    flag = action.option_strings[0]
    name_cell = (
        f'<code style="color: darkblue">{escape_html(flag)}</code>'
        f'<br/><span style="color: darkgreen;">'
        f'({type_label(action)})</span>'
    )
    return (
        '<tr>'
        f'<td class="tableblock halign-left valign-top">{name_cell}</td>'
        f'<td class="tableblock halign-left valign-top">'
        f'{render_description_cell(action)}</td>'
        '</tr>'
    )


def render_group_table(actions):
    rows = "\n".join(render_row(a) for a in actions)
    return (
        '++++\n'
        '<table class="tableblock frame-all grid-all stretch">\n'
        '  <colgroup>\n'
        '    <col style="width: 25%;">\n'
        '    <col style="width: 75%;">\n'
        '  </colgroup>\n'
        '  <thead>\n'
        '    <tr>\n'
        '      <th class="tableblock halign-left valign-top">Name</th>\n'
        '      <th class="tableblock halign-left valign-top">Description</th>\n'
        '    </tr>\n'
        '  </thead>\n'
        '  <tbody>\n'
        f'{rows}\n'
        '  </tbody>\n'
        '</table>\n'
        '++++\n'
    )


def render(parser):
    out = [HEADER]
    for group in parser._action_groups:
        if group.title in ("positional arguments", "options",
                           "optional arguments"):
            continue
        actions = [a for a in group._group_actions
                   if not isinstance(a, argparse._VersionAction)]
        if not actions:
            continue
        out.append(f"[#{slugify(group.title)}]\n")
        out.append(f"=== {group.title}\n\n")
        out.append(render_group_table(actions))
        out.append("\n")
    return "".join(out).rstrip() + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--update", action="store_true",
        help="Write the partial to disk.")
    mode.add_argument(
        "--check", action="store_true",
        help="Exit non-zero if the partial on disk is out of date.")
    args = p.parse_args()

    parser = build_arg_parser()
    missing = [
        a.dest for g in parser._action_groups for a in g._group_actions
        if a.dest and a.dest not in OPTION_DETAILS
        and a.dest not in ("help", "version")
    ]
    # Each argparse `dest` appears once per add_argument call, including for
    # paired flags that share a dest; dedupe so the message lists each name
    # once.
    missing = sorted(set(missing))
    if missing:
        print(
            "FAIL: util/bootstrap/src/core/option_help.py is missing "
            f"OPTION_DETAILS entries for: {', '.join(missing)}",
            file=sys.stderr)
        return 1

    rendered = render(parser)

    if args.update:
        os.makedirs(os.path.dirname(PARTIAL_PATH), exist_ok=True)
        with open(PARTIAL_PATH, "w", encoding="utf-8") as f:
            f.write(rendered)
        print(f"Wrote {PARTIAL_PATH}")
        return 0

    if not os.path.exists(PARTIAL_PATH):
        print(f"FAIL: {PARTIAL_PATH} does not exist", file=sys.stderr)
        return 1
    with open(PARTIAL_PATH, encoding="utf-8") as f:
        on_disk = f.read()
    if on_disk != rendered:
        print(
            f"FAIL: {PARTIAL_PATH} is out of date. Run "
            f"`python util/docs/generate_bootstrap_options.py --update`.",
            file=sys.stderr)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
