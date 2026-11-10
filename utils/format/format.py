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

"""Format the lines a change touched in the `.hpp` and `.cpp` files.

With no paths it works on the files changed since the merge base of HEAD with
the base ref (see `changed_lines.py`) and rewrites only their changed lines,
in three steps. Each header whose guard does not match its path gets the guard
of its path (`MRDOCS_API_` for include/mrdocs, `MRDOCS_LIB_` for src/mrdocs,
`MRDOCS_TOOL_` for tools/mrdocs, `MRDOCS_TEST_` for tests and
`MRDOCS_TEST_SUITE_` for libs/test_suite), when one of the three guard lines
changed or the file's path is new (an added or renamed header). Each quoted include of a public header becomes the angle form under
include/, such as `<mrdocs/Support/Path.hpp>`, and a private header stays
quoted. Then clang-format, with the repository's `.clang-format`, formats the
changed lines, and sorts the include blocks those lines touch. Lines the
change did not touch stay as they are. Without `clang-format` on the path the
last line of the output is the warning `warning: clang-format not found on
PATH; formatting did not run (install clang-format 21)`, and the exit status
does not change.

Named paths are limited to their changed lines too. `--all` formats every
line of every file instead: of the five trees, which are include/mrdocs,
src/mrdocs, tools/mrdocs, tests and libs/test_suite, or of the named paths.
`--base REF` picks another base. Files under `tests/golden/fixtures` are never
touched, in any mode: they are the sources the golden tests parse.

With `--check` nothing is written. Each line a rewrite would change prints as
`file:line: message (rule)` and the exit status is 1 when any line would
change or clang-format fails on a file. A change to a line the branch did not touch, such as the include sort of a
block a new include joined, is blamed on the nearest touched line, so a clean
`--check` means a write changes nothing. The rule is `clang-format` for a line
clang-format would change and `header-rules` for an include or a header guard. With `--sarif FILE` the lines
are also written to FILE as SARIF.

    python3 utils/format/format.py
    python3 utils/format/format.py --base HEAD~3
    python3 utils/format/format.py include/mrdocs/Support/Reporter.hpp
    python3 utils/format/format.py --check
    python3 utils/format/format.py --all
"""

import argparse
import os
import sys
import subprocess
import shutil
from typing import List, NamedTuple, Optional, Set, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
# changed_lines.py, header_rules.py and sarif.py live in utils/lint, next to the
# checks; the dependency runs from format to lint only.
sys.path.insert(0, os.path.join(HERE, "..", "lint"))

import changed_lines
import header_rules
import sarif
from header_rules import Finding, RULE_HEADER, SKIPPED_TREES

# ---------- configuration ----------
CLANG_FORMAT_EXE = shutil.which("clang-format")  # resolve once
REPO_ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RULE_CLANG_FORMAT = "clang-format"
CLANG_FORMAT_WARNING = "warning: clang-format not found on PATH; formatting did not run (install clang-format 21)"

# ---------- clang-format ----------
def _clang_format(path: str, text: str, touched: Optional[Set[int]]) -> Optional[str]:
    """
    Return `text` formatted by clang-format with the .clang-format found from
    'path', on the lines in 'touched' only (None means every line).
    Returns None when clang-format is missing, and raises RuntimeError when it fails.
    """
    if CLANG_FORMAT_EXE is None:
        return None
    cmd = [CLANG_FORMAT_EXE, "-style=file", f"--assume-filename={path}"]
    if touched is not None:
        for first, last in _runs(sorted(touched)):
            cmd.append(f"--lines={first}:{last}")
    try:
        done = subprocess.run(cmd, check=True, input=text.encode("utf-8"),
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"clang-format failed: {e.stderr.decode('utf-8', 'replace').strip()}")
    return done.stdout.decode("utf-8")

def _runs(numbers: List[int]) -> List[Tuple[int, int]]:
    """Group sorted numbers into (first, last) runs of consecutive numbers."""
    runs: List[List[int]] = []
    for n in numbers:
        if runs and n == runs[-1][1] + 1:
            runs[-1][1] = n
        else:
            runs.append([n, n])
    return [(a, b) for a, b in runs]

def _nearest(line: int, touched: Optional[Set[int]]) -> int:
    """
    Return 'line' when the change touched it, else the touched line nearest to it
    (the earlier one on a tie). A rewrite that moves a line the change did not
    touch, such as the include sort of a block a new include joined, is the
    change's doing, so the check blames the touched line and fails as the write does.
    """
    if not touched or line in touched:
        return line
    return min(touched, key=lambda t: (abs(t - line), t))

# ---------- per-file transform ----------
class Result(NamedTuple):
    status: str            # "ok", "empty" or "would change"/"updated" with the steps
    findings: List[Finding]
    note: str = ""

def process_file(path: str, guard_base: Optional[str], guard_prefix: str, include_root: str,
                 check: bool = False, touched: Optional[Set[int]] = None,
                 path_changed: bool = False) -> Result:
    """
    1) For .hpp: if the canonical guard differs from the guard, rewrite it
       (only when one of its three lines is in 'touched', or when the file's
       path is new, so the guard has to match the path it now has).
    2) For .hpp & .cpp: normalize the includes on the lines in 'touched'.
    3) clang-format the lines in 'touched', which also sorts the include blocks
       those lines touch.

    'touched' holds the 1-based lines the change touched; None means every line.
    Lines are the ones git counts, split on "\\n" only.
    A guard_base of None means the file is outside the known trees: its guard is left alone.
    With check=True nothing is written. The findings hold one entry per line the
    steps would change, blamed on the line that changes, or that moves.
    """
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        lines = changed_lines.split_lines(f.read(), keepends=True)
    if not lines:
        return Result("empty", [])
    if touched is not None and not touched and not path_changed:
        return Result("ok", [])

    findings, steps = header_rules.apply(lines, path, guard_base, guard_prefix, include_root,
                                         touched, path_changed)

    text = "".join(lines)
    note = ""
    formatted = None
    if touched is None or touched:
        try:
            formatted = _clang_format(path, text, touched)
        except RuntimeError as e:
            note = f" ({e})"
        if formatted is None and not note:
            note = " (clang-format not found)"
    if formatted is not None and formatted != text:
        before = changed_lines.split_lines(text)
        after = changed_lines.split_lines(formatted)
        blamed = changed_lines.blamed_lines(before, after, touched) or [max(len(before), 1)]
        for line in blamed:
            findings.append(Finding(_nearest(line, touched), RULE_CLANG_FORMAT,
                                    "clang-format would change this line"))
        steps.append("clang-format")
        text = formatted

    if touched is not None:
        findings = [f for f in findings if f.line in touched or f.rule == RULE_HEADER]
    findings = sorted(set(findings), key=lambda f: (f.line, f.rule))

    if not steps:
        return Result("ok", findings, note)
    if check:
        if not findings:
            return Result("ok", findings, note)
        return Result("would change (" + ", ".join(steps) + ")", findings, note)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    return Result("updated (" + "; ".join(steps) + ")", findings, note)

SARIF_RULES = [RULE_HEADER, RULE_CLANG_FORMAT]

def run(paths: List[str], check: bool, repo_root: str,
        sarif_path: Optional[str] = None,
        change: Optional[changed_lines.Change] = None) -> int:
    """
    Format, or with check=True report, the files in 'paths', or the files the
    change touched when there are none. 'change' limits the work to the lines
    it touched; None means every line of every file.
    """
    work, status = header_rules.collect_work(paths, repo_root, change)
    if status:
        return status

    log = sarif.Log("format", SARIF_RULES)
    total = updated = skipped = 0
    warned = False
    for f, base, prefix, inc_root, touched, new_path in work:
        total += 1
        rel = os.path.relpath(f, repo_root).replace(os.sep, "/")
        try:
            result = process_file(f, base, prefix, inc_root, check=check, touched=touched,
                                 path_changed=new_path)
        except Exception as e:
            result = Result(f"skipped (error: {e})", [])
        # A clang-format error leaves the file unjudged, so it counts as skipped.
        format_failed = result.note.startswith(" (clang-format failed")
        if result.status.startswith(("updated", "would change")):
            updated += 1
        elif result.status.startswith("skipped") or format_failed:
            skipped += 1
        if result.note == " (clang-format not found)":
            warned = True
        elif result.note:
            print(f"{rel}:{result.note}")
            if check and format_failed:
                log.add(RULE_CLANG_FORMAT, result.note.strip(" ()"), uri=rel)
        if check:
            for finding in result.findings:
                print(f"{rel}:{finding.line}: {finding.message} ({finding.rule})")
                log.add(finding.rule, finding.message, uri=rel, start_line=finding.line)
            if result.status.startswith("skipped"):
                print(f"{rel}: {result.status}")
                log.add(RULE_HEADER, result.status, uri=rel)
        elif result.status not in ("ok", "empty"):
            print(f"{rel}: {result.status}")

    verb = "would change" if check else "updated"
    print(f"\nAll done. {total} files total, {updated} {verb}, {skipped} skipped.")
    if warned:
        print(CLANG_FORMAT_WARNING)
        if sarif_path:
            log.add(RULE_CLANG_FORMAT, CLANG_FORMAT_WARNING[len("warning: "):], level="warning")
    if sarif_path:
        log.write(sarif_path)
    if check and (updated or skipped):
        return 1
    return 0

def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Format the lines a change touched: clang-format, the include "
                    "spelling and the header guards. Without --all it works on the "
                    "files changed since the merge base and touches only their "
                    "changed lines.")
    ap.add_argument("--check", action="store_true",
                    help="rewrite nothing; list the lines that would change and exit 1 if any would")
    changed_lines.add_arguments(ap)
    ap.add_argument("paths", nargs="*", metavar="PATH",
                    help="files, or directories walked recursively, limited to their "
                         "changed lines unless --all; default: every file changed in the "
                         "five source trees (include/mrdocs, src/mrdocs, tools/mrdocs, "
                         "tests, libs/test_suite)")
    ap.add_argument("--sarif", metavar="FILE",
                    help="with --check, also write the lines that would change to FILE as SARIF")
    args = ap.parse_args(argv)
    if args.sarif and not args.check:
        ap.error("--sarif needs --check")
    repo_root = REPO_ROOT
    try:
        change = changed_lines.from_args(args, repo_root)
    except changed_lines.NoBase as e:
        print(f"format: {e}; use --base REF or --all")
        return 1
    return run([os.path.abspath(p) for p in args.paths], args.check, repo_root,
               args.sarif, change)

if __name__ == "__main__":
    sys.exit(main())
