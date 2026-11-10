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

"""Cap the length of every documentation page.

A page that runs past a few thousand words stops being readable in one sitting,
and its sidebar entry hides what it holds. A long page becomes an index with
subpages. The check counts the whitespace-separated words of each
docs/modules/ROOT/pages/**/*.adoc page, leaving out code blocks (`----` and
`....` fences, which also cover `[source]` blocks), comment lines and blocks
(`//` and `////`), passthrough blocks (`++++`) and `include::` lines, and
prints one line per page over the cap. It exits 1 when any page is over, and
prints nothing and exits 0 otherwise. With `--sarif FILE` it also writes the
pages over the cap as a SARIF file, under the rule `page-length` of the
documentation invariants page.

The cap is MAX_WORDS, 3000 words: the first multiple of 500 above the longest
page, which is Defining Extensions at 2764 words.

By default it checks only the pages the branch changed since the merge base of
HEAD with the base ref, untracked ones included (see `changed_lines.py`);
`--base REF` picks another base and `--all` checks every page. The root is the
repository that holds this script, so the working directory does not matter:

    python3 -I utils/lint/check_page_length.py
    python3 -I utils/lint/check_page_length.py --base HEAD~3
    python3 -I utils/lint/check_page_length.py --all
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import changed_lines
import sarif

RULE = "page-length"

MAX_WORDS = 3000

PAGES_DIR = Path("docs/modules/ROOT/pages")

ROOT = Path(__file__).resolve().parents[2]


def count_words(text):
    """Count the words of `text`, leaving out code blocks and include lines."""
    words = 0
    fence = None
    for line in changed_lines.split_lines(text):
        stripped = line.rstrip()
        if fence is None:
            if stripped in ("////", "++++") or (
                    len(stripped) >= 4 and set(stripped) in ({"-"}, {"."})):
                fence = stripped
                continue
            if stripped.startswith("include::"):
                continue
            if stripped.startswith("//"):
                continue
            words += len(line.split())
        elif stripped == fence:
            fence = None
    return words


def find_pages(root):
    """Return the .adoc pages under the pages directory of `root`, sorted."""
    return sorted((root / PAGES_DIR).rglob("*.adoc"))


def find_problems(root, change=None):
    """Return a (path, message) pair per page under `root` over the cap, the
    path relative to `root`. `change` limits the pages to those it touched."""
    problems = []
    pages = find_pages(root)
    if change is not None:
        touched = set(change.files())
        pages = [p for p in pages if p.relative_to(root).as_posix() in touched]
    for path in pages:
        n = count_words(path.read_text(encoding="utf-8", errors="replace"))
        if n > MAX_WORDS:
            rel = path.relative_to(root).as_posix()
            problems.append((
                rel,
                f"{rel}: {n} words, the cap is {MAX_WORDS} (split the page into "
                "an index and subpages)",
            ))
    return problems


def check(root, change=None):
    """Return one message per page under `root` that is over the cap."""
    return [message for _, message in find_problems(root, change)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sarif", metavar="FILE",
                        help="also write the findings to FILE as SARIF")
    parser.add_argument("--counts", action="store_true",
                        help="print the word count of every page, longest first")
    changed_lines.add_arguments(parser)
    args = parser.parse_args(argv)
    if args.counts:
        counts = [(count_words(p.read_text(encoding="utf-8", errors="replace")),
                   p.relative_to(ROOT).as_posix()) for p in find_pages(ROOT)]
        for n, rel in sorted(counts, reverse=True):
            print(f"{n:6d} {rel}")
        return 0
    try:
        change = changed_lines.from_args(args, ROOT)
    except changed_lines.NoBase as error:
        print("check_page_length: %s; use --base REF or --all" % error)
        return 1
    problems = find_problems(ROOT, change)
    for _, line in problems:
        print(line)
    if args.sarif:
        log = sarif.Log("check_page_length", [RULE])
        for rel, line in problems:
            log.add(RULE, line, uri=rel)
        log.write(args.sarif)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
