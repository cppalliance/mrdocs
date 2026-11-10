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

"""Check the two naming rules of the C++ headers on the lines a change touched.

A header guard matches the path of its header, and a public header is
included as `<mrdocs/...>`. The rules live in `header_rules.py`, which
`utils/format/format.py` imports to fix the same lines. The check writes
nothing: each line that breaks a rule prints as
`file:line: message (header-rules)` and the exit status is 1 when any does.

The change is everything since the merge base of HEAD with the default branch
of `upstream` (see `changed_lines.py`); `--base REF` picks another base and
`--all` checks every line of the five source trees.

    python3 -I utils/lint/check_header_rules.py
    python3 -I utils/lint/check_header_rules.py --base HEAD~3
    python3 -I utils/lint/check_header_rules.py --all
    python3 -I utils/lint/check_header_rules.py include/mrdocs/Platform.hpp
"""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import changed_lines
import header_rules

ROOT = HERE.parents[1]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sarif", metavar="FILE",
                        help="also write the findings to FILE as SARIF")
    changed_lines.add_arguments(parser)
    parser.add_argument("paths", nargs="*", metavar="PATH",
                        help="files or directories, limited to their changed lines "
                             "unless --all; default: the five source trees")
    args = parser.parse_args(argv)
    try:
        change = changed_lines.from_args(args, str(ROOT))
    except changed_lines.NoBase as error:
        print("check_header_rules: %s; use --base REF or --all" % error)
        return 1
    return header_rules.check([str(Path(p).resolve()) for p in args.paths], str(ROOT), args.sarif, change)


if __name__ == "__main__":
    sys.exit(main())
