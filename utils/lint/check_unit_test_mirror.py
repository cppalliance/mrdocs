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

"""Check that every unit test file mirrors the header it tests.

A unit test `tests/unit/<path>.cpp` passes when `<path>.hpp` exists under
`include/mrdocs/` or `src/mrdocs/`, spelled with the same case. A library unit
test `libs/<lib>/tests/unit/<path>.cpp` passes when `<path>.hpp` exists under
`libs/<lib>/include/mrdocs/`. A test driver, a file named `UnitMain.cpp` or
`*TestMain.cpp`, is exempt, and so is every file outside a `tests/unit`
directory, such as the golden tests. It prints one line per file that has no
header, and exits 1 when any does. With `--sarif FILE` it also writes the
findings as a SARIF file, under the rule `unit-test-mirror`, which the tests
layout page describes.

By default it judges only the unit tests the branch added or renamed, and the
tests whose mirrored header the branch deleted or renamed, since the
merge base of HEAD with the base ref, untracked ones included (see
`changed_lines.py`); `--base REF` picks another base and `--all` judges every
unit test:

    python3 -I utils/lint/check_unit_test_mirror.py
    python3 -I utils/lint/check_unit_test_mirror.py --base HEAD~3
    python3 -I utils/lint/check_unit_test_mirror.py --all
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import changed_lines
import sarif

RULE = "unit-test-mirror"

UNIT_DIR = "tests/unit"
HEADER_DIRS = ("include/mrdocs", "src/mrdocs")
LIB_HEADER_DIR = "include/mrdocs"

ROOT = Path(__file__).resolve().parents[2]


def _is_driver(name):
    """Return whether the file name `name` is a test driver."""
    return name == "UnitMain.cpp" or name.endswith("TestMain.cpp")


def _split(rel):
    """Return `(header directories, path inside the unit directory)` for a unit test, else None.

    The unit directory is `tests/unit` for the main tests, whose headers live
    under `include/mrdocs` or `src/mrdocs`, and `libs/<lib>/tests/unit` for a
    library, whose headers live under that library's `include/mrdocs`.
    """
    parts = Path(rel).parts
    if not rel.endswith(".cpp") or _is_driver(parts[-1]):
        return None
    if len(parts) >= 3 and "/".join(parts[:2]) == UNIT_DIR:
        return HEADER_DIRS, "/".join(parts[2:])
    if len(parts) >= 5 and parts[0] == "libs" and "/".join(parts[2:4]) == UNIT_DIR:
        return ("/".join(parts[:2]) + "/" + LIB_HEADER_DIR,), "/".join(parts[4:])
    return None


def _is_unit_test(rel):
    """Return whether the repo-relative posix path `rel` is a unit test source to judge."""
    return _split(rel) is not None


def _exists_exactly(directory, relative):
    """Return whether the file `relative` exists under `directory`, spelled with the same case.

    The file system may ignore case, so each name is compared with the listing
    of its directory.
    """
    current = Path(directory)
    for name in Path(relative).parts:
        try:
            if name not in {entry.name for entry in current.iterdir()}:
                return False
        except OSError:
            return False
        current = current / name
    return current.is_file()


def find_tests(root, change=None):
    """Return the repo-relative posix paths of the unit tests to judge.

    With a `change`, only the tests it added, renamed or left untracked and the
    tests whose mirrored header path it touched (a deleted or renamed header
    leaves its test with no header); without one, every unit test under `root`.
    """
    if change is not None:
        touched = set(change.paths())
        chosen = {rel for rel in change.files() if _is_unit_test(rel) and change.is_new_path(rel)}
        for rel in find_tests(root):
            directories, inside = _split(rel)
            header = Path(inside).with_suffix(".hpp").as_posix()
            if any(directory + "/" + header in touched for directory in directories):
                chosen.add(rel)
        return sorted(chosen)
    found = []
    for pattern in (UNIT_DIR, "libs/*/" + UNIT_DIR):
        for base in Path(root).glob(pattern):
            found += [p.relative_to(root).as_posix() for p in base.rglob("*.cpp")]
    return sorted(rel for rel in found if _is_unit_test(rel))


def find_problems(root, change=None):
    """Return a (path, message) pair per unit test under `root` with no header to mirror."""
    problems = []
    for rel in find_tests(root, change):
        directories, inside = _split(rel)
        header = Path(inside).with_suffix(".hpp").as_posix()
        if not any(_exists_exactly(Path(root) / directory, header) for directory in directories):
            where = " or ".join("%s/" % directory for directory in directories)
            problems.append((
                rel,
                f"{rel}: no header {header} under {where}; "
                "a unit test mirrors the header it tests",
            ))
    return problems


def check(root, change=None):
    """Return one message per unit test under `root` with no header to mirror."""
    return [message for _, message in find_problems(root, change)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sarif", metavar="FILE",
                        help="also write the findings to FILE as SARIF")
    changed_lines.add_arguments(parser)
    args = parser.parse_args(argv)
    try:
        change = changed_lines.from_args(args, ROOT)
    except changed_lines.NoBase as error:
        print("check_unit_test_mirror: %s; use --base REF or --all" % error)
        return 1
    problems = find_problems(ROOT, change)
    for _, line in problems:
        print(line)
    if args.sarif:
        log = sarif.Log("check_unit_test_mirror", [RULE])
        for rel, line in problems:
            log.add(RULE, line, uri=rel)
        log.write(args.sarif)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
