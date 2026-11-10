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

"""Cap the length of every README.md in the tree.

A README says what a directory holds and how it works. The documentation
carries the rest. The check counts the whitespace-separated words of each
tracked README.md outside third-party/, build/, local/, node_modules/ and .git/,
leaving fenced code blocks out, and prints one line per file over the cap.
It exits 1 when any file is over, and prints nothing and exits 0 otherwise.
With `--sarif FILE` it also writes the files over the cap as a SARIF file,
under the rule `readme-length` of the documentation invariants page.

By default it checks only the READMEs the branch changed since the merge
base of HEAD with the base ref, untracked ones included (see
`changed_lines.py`); `--base REF` picks another base and `--all` checks every
README. The files come from git, so ignored trees never count, and the root is
the repository that holds this script, so the working directory does not
matter:

    python3 -I utils/lint/check_readme_length.py
    python3 -I utils/lint/check_readme_length.py --base HEAD~3
    python3 -I utils/lint/check_readme_length.py --all
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import changed_lines
import sarif

RULE = "readme-length"

# A README of a few short paragraphs, with no file-by-file walk, fits in 180 words.
MAX_WORDS = 180

SKIP_DIRS = {"third-party", "build", "local", "node_modules", ".git"}

ROOT = Path(__file__).resolve().parents[2]


def count_words(text):
    """Count the words of `text`, leaving out fenced code blocks."""
    words = 0
    fence = None
    for line in changed_lines.split_lines(text):
        stripped = line.strip()
        marker = stripped[:3]
        if fence is None:
            if marker in ("```", "~~~"):
                fence = marker
                continue
            words += len(line.split())
        elif stripped.startswith(fence):
            fence = None
    return words


def _skipped(rel):
    return any(part in SKIP_DIRS for part in rel.parts[:-1])


def _git_readmes(root):
    out = subprocess.run(
        ["git", "ls-files", "-z", "--", "README.md", "*/README.md"],
        cwd=root, capture_output=True, check=True,
    ).stdout.decode("utf-8", errors="replace")
    return [Path(p) for p in out.split("\0") if p]


def _walk_readmes(root):
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
        if "README.md" in filenames:
            found.append((Path(dirpath) / "README.md").relative_to(root))
    return found


def find_readmes(root, change=None):
    """Return the README.md files under `root`, skipping the excluded
    directories: those `change` touched, or the tracked ones when it is None.
    Outside a git checkout, walk the tree instead."""
    if change is not None:
        return [root / rel for rel in change.files()
                if Path(rel).name == "README.md" and not _skipped(Path(rel))]
    try:
        rels = _git_readmes(root)
    except (OSError, subprocess.CalledProcessError):
        rels = _walk_readmes(root)
    return [root / rel for rel in rels if not _skipped(rel) and (root / rel).is_file()]


def check(root, change=None):
    """Return one message per README under `root` that is over the cap."""
    return [message for _, message in find_problems(root, change)]


def find_problems(root, change=None):
    """Return a (path, message) pair per README under `root` over the cap, the
    path relative to `root`. `change` limits the READMEs to those it touched."""
    problems = []
    for path in find_readmes(root, change):
        text = path.read_text(encoding="utf-8", errors="replace")
        n = count_words(text)
        if n > MAX_WORDS:
            rel = path.relative_to(root).as_posix()
            problems.append((
                rel,
                f"{rel}: {n} words, the cap is {MAX_WORDS} (a README says what "
                "the directory holds and how it works; the docs carry the rest)",
            ))
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sarif", metavar="FILE",
                        help="also write the findings to FILE as SARIF")
    changed_lines.add_arguments(parser)
    args = parser.parse_args(argv)
    try:
        change = changed_lines.from_args(args, ROOT)
    except changed_lines.NoBase as error:
        print("check_readme_length: %s; use --base REF or --all" % error)
        return 1
    problems = find_problems(ROOT, change)
    for _, line in problems:
        print(line)
    if args.sarif:
        log = sarif.Log("check_readme_length", [RULE])
        for rel, line in problems:
            log.add(RULE, line, uri=rel)
        log.write(args.sarif)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
