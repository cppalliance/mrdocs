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

"""Forbid section titles that ask a question, and links to generated ids.

A title such as "What goes elsewhere" announces a question and leaves the
sidebar and the table of contents saying nothing about the concept the section
discusses. The title names the concept instead: "Content that belongs
elsewhere". The check reads every docs/modules/ROOT/pages/**/*.adoc page and
looks at each section title (`==` to `======`; the `=` page title is left
alone) whose first word is What, Where, When, How, Why, Which or Who, or that
opens with "In what", in any case, after any leading `[[anchor]]` and emphasis
marks, and at each title that ends with a question mark. Lines inside code and
literal blocks (`----` and `....` fences), comment lines and blocks (`//` and
`////`) and passthrough blocks (`++++`), each fence four or more characters
long, are not titles. It prints one line per title, exits 1 when any title is
found, and prints nothing and exits 0 otherwise. With `--sarif FILE` it also
writes the titles as a SARIF file, under the rule `section-title` of the
documentation invariants page.

The check also fails on a link to a section by the id Asciidoctor generates
from its title: an `xref:` or a `<<...>>` whose fragment starts with `_`, such
as `xref:contribute/workflow.adoc#_build_the_documentation[Setup]`. Renaming
the title silently breaks such a link. The target section gets an explicit
anchor, `[#build-docs]` above its title, and the link uses that anchor. The
same blocks are skipped, and each finding is on the line of the link.

By default it checks only the titles on lines the branch changed since the
merge base of HEAD with the base ref, in pages it added or edited (see
`changed_lines.py`); `--base REF` picks another base and `--all` checks every
title. The root is the repository that holds this script, so the working
directory does not matter:

    python3 -I utils/lint/check_section_titles.py
    python3 -I utils/lint/check_section_titles.py --base HEAD~3
    python3 -I utils/lint/check_section_titles.py --all
"""

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import changed_lines
import sarif

RULE = "section-title"

PAGES_DIR = Path("docs/modules/ROOT/pages")

ROOT = Path(__file__).resolve().parents[2]

SECTION = re.compile(r"^={2,6}\s+(\S.*?)\s*$")

QUESTION_WORD = re.compile(
    r"(?:\[\[[^\]]*\]\])?[*_`]*(What|Where|When|How|Why|Which|Who|In\s+what)(?![a-z0-9-])",
    re.IGNORECASE)

GENERATED_LINK = re.compile(
    r"xref:[^\s\[#]*#(?P<id>_[^\s\[\]]*)\["
    r"|<<(?:[^\s<>,#]*#)?(?P<id2>_[^\s<>,]*)(?:,[^<>]*)?>>")

FENCE_CHARS = ({"-"}, {"."}, {"/"}, {"+"})


def prose_lines(text):
    """Yield a (line number, line) pair per line of `text` outside the code,
    literal, comment and passthrough blocks and the `//` comment lines, with
    trailing whitespace removed."""
    fence = None
    for number, line in enumerate(text.split("\n"), 1):
        stripped = line.rstrip()
        if fence is not None:
            if stripped == fence:
                fence = None
            continue
        if len(stripped) >= 4 and set(stripped) in FENCE_CHARS:
            fence = stripped
            continue
        if stripped.startswith("//"):
            continue
        yield number, stripped


def find_titles(text):
    """Return a (line number, title, reason) triple per section title in
    `text` that asks a question. The reason is `opens with "How"`, with the
    opening word as written, or `ends with "?"`."""
    found = []
    for number, stripped in prose_lines(text):
        match = SECTION.match(stripped)
        if match:
            title = match.group(1)
            word = QUESTION_WORD.match(title)
            if word:
                found.append((number, title, 'opens with "%s"' % word.group(1)))
            elif title.rstrip("*_` ").endswith("?"):
                found.append((number, title, 'ends with "?"'))
    return found


def find_generated_links(text):
    """Return a (line number, fragment) pair per link in `text` whose
    fragment starts with `_`: an `xref:` or a `<<...>>` that targets the id
    Asciidoctor generates from a section title."""
    found = []
    for number, stripped in prose_lines(text):
        for match in GENERATED_LINK.finditer(stripped):
            found.append((number, match.group("id") or match.group("id2")))
    return found


def find_pages(root):
    """Return the .adoc pages under the pages directory of `root`, sorted."""
    return sorted((root / PAGES_DIR).rglob("*.adoc"))


def find_problems(root, change=None):
    """Return a (path, line, message) triple per offending title under `root`,
    the path relative to `root`. `change` limits the titles to the lines it
    touched."""
    problems = []
    for path in find_pages(root):
        rel = path.relative_to(root).as_posix()
        touched = None
        if change is not None:
            if rel not in set(change.files()):
                continue
            touched = change.lines(rel)
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, title, reason in find_titles(text):
            if touched is not None and number not in touched:
                continue
            problems.append((
                rel,
                number,
                f'{rel}:{number}: section title "{title}" {reason}; '
                "name the concept the section discusses instead, such as "
                '"Content that belongs elsewhere"',
            ))
        for number, fragment in find_generated_links(text):
            if touched is not None and number not in touched:
                continue
            problems.append((
                rel,
                number,
                f'{rel}:{number}: link to the generated id "{fragment}"; '
                "give the target section an explicit anchor, such as "
                f'"[#{fragment.strip("_").replace("_", "-")}]" above its title, '
                "and link to that",
            ))
    problems.sort(key=lambda problem: (problem[0], problem[1]))
    return problems


def check(root, change=None):
    """Return one message per offending section title under `root`."""
    return [message for _, _, message in find_problems(root, change)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sarif", metavar="FILE",
                        help="also write the findings to FILE as SARIF")
    changed_lines.add_arguments(parser)
    args = parser.parse_args(argv)
    try:
        change = changed_lines.from_args(args, ROOT)
    except changed_lines.NoBase as error:
        print("check_section_titles: %s; use --base REF or --all" % error)
        return 1
    problems = find_problems(ROOT, change)
    for _, _, line in problems:
        print(line)
    if args.sarif:
        log = sarif.Log("check_section_titles", [RULE])
        for rel, number, line in problems:
            log.add(RULE, line, uri=rel, start_line=number)
        log.write(args.sarif)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
