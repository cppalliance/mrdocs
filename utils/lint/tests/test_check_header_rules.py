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


"""Tests for utils/lint/check_header_rules.py, run on scratch repositories."""

import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import check_header_rules
from tests import gitrepo

BAD_GUARD = "#ifndef WRONG\n#define WRONG\nint x;\n#endif\n"
UGLY = "int   x ;\n"


class HeaderRulesTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(os.path.join(self.tmp, "repo"))
        gitrepo.write(self.repo, "include/mrdocs/Old.hpp", "#ifndef MRDOCS_API_OLD_HPP\n"
                      "#define MRDOCS_API_OLD_HPP\nint a;\n#endif // MRDOCS_API_OLD_HPP\n")
        gitrepo.commit_base(self.repo)
        original = check_header_rules.ROOT
        self.addCleanup(setattr, check_header_rules, "ROOT", original)
        check_header_rules.ROOT = type(original)(self.repo)

    def _main(self, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_header_rules.main(argv)
        return code, out.getvalue()

    def test_a_wrong_guard_fails_and_nothing_is_written(self):
        gitrepo.write(self.repo, "include/mrdocs/New.hpp", BAD_GUARD)
        code, out = self._main([])
        self.assertEqual(code, 1)
        self.assertIn("include/mrdocs/New.hpp:1: include guard WRONG would become "
                      "MRDOCS_API_NEW_HPP (header-rules)", out)
        with open(os.path.join(self.repo, "include/mrdocs/New.hpp")) as f:
            self.assertEqual(f.read(), BAD_GUARD)

    def test_unformatted_code_is_not_a_finding(self):
        gitrepo.write(self.repo, "src/mrdocs/Ugly.cpp", UGLY)
        code, out = self._main([])
        self.assertEqual(code, 0)
        self.assertNotIn("clang-format", out)

    def test_a_clean_tree_passes_with_all(self):
        self.assertEqual(self._main(["--all"])[0], 0)


if __name__ == "__main__":
    unittest.main()
