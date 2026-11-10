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

"""Tests for utils/lint/check_unit_test_mirror.py, run on scratch trees."""

import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import changed_lines
import check_unit_test_mirror
from tests import gitrepo

MISSING = ("tests/unit/Extensions/ScriptContext.cpp: no header Extensions/ScriptContext.hpp "
           "under include/mrdocs/ or src/mrdocs/; a unit test mirrors the header it tests")


class UnitTestMirrorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(os.path.join(self.tmp, "repo"))
        gitrepo.write(self.repo, "include/mrdocs/Config.hpp", "")
        gitrepo.write(self.repo, "src/mrdocs/Extensions/ExtensionContext.hpp", "")
        gitrepo.write(self.repo, "tests/unit/UnitMain.cpp", "")
        gitrepo.write(self.repo, "tests/unit/Config.cpp", "")
        gitrepo.write(self.repo, "tests/unit/Extensions/ExtensionContext.cpp", "")
        gitrepo.write(self.repo, "tests/unit/Old.cpp", "")
        gitrepo.commit_base(self.repo)

    def _change(self):
        return changed_lines.Change(self.repo, changed_lines.resolve_base(self.repo)[0])

    def test_with_all_only_the_test_without_a_header_is_reported(self):
        self.assertEqual(check_unit_test_mirror.check(self.repo, None),
                         ["tests/unit/Old.cpp: no header Old.hpp under include/mrdocs/ or src/mrdocs/; "
                          "a unit test mirrors the header it tests"])

    def test_a_new_test_with_no_header_is_reported(self):
        gitrepo.write(self.repo, "tests/unit/Extensions/ScriptContext.cpp", "")
        self.assertEqual(check_unit_test_mirror.check(self.repo, self._change()), [MISSING])

    def test_the_driver_and_files_outside_the_unit_tests_are_not_judged(self):
        gitrepo.write(self.repo, "tests/unit/Notes.md", "")
        gitrepo.write(self.repo, "tests/golden/Other.cpp", "")
        gitrepo.write(self.repo, "libs/dom/tests/Dom.cpp", "")
        self.assertEqual(check_unit_test_mirror.check(self.repo, self._change()), [])

    def test_a_test_the_branch_did_not_add_passes(self):
        gitrepo.write(self.repo, "tests/unit/Old.cpp", "edited\n")
        self.assertEqual(check_unit_test_mirror.check(self.repo, self._change()), [])

    def test_a_header_spelled_with_other_case_does_not_count(self):
        gitrepo.write(self.repo, "include/mrdocs/widget.hpp", "")
        gitrepo.write(self.repo, "src/mrdocs/Extensions/gadget.hpp", "")
        gitrepo.write(self.repo, "tests/unit/Widget.cpp", "")
        gitrepo.write(self.repo, "tests/unit/Extensions/Gadget.cpp", "")
        problems = check_unit_test_mirror.check(self.repo, self._change())
        self.assertEqual(len(problems), 2)
        self.assertTrue(problems[0].startswith("tests/unit/Extensions/Gadget.cpp: no header"))
        self.assertTrue(problems[1].startswith("tests/unit/Widget.cpp: no header"))

    def test_a_library_test_mirrors_a_header_of_its_own_library(self):
        gitrepo.write(self.repo, "libs/dom/include/mrdocs/Dom/Value.hpp", "")
        gitrepo.write(self.repo, "libs/dom/tests/unit/Dom/Value.cpp", "")
        gitrepo.write(self.repo, "libs/dom/tests/unit/Dom/Missing.cpp", "")
        gitrepo.write(self.repo, "libs/polyfill/tests/unit/Dom/Value.cpp", "")
        self.assertEqual(check_unit_test_mirror.check(self.repo, self._change()), [
            "libs/dom/tests/unit/Dom/Missing.cpp: no header Dom/Missing.hpp under "
            "libs/dom/include/mrdocs/; a unit test mirrors the header it tests",
            "libs/polyfill/tests/unit/Dom/Value.cpp: no header Dom/Value.hpp under "
            "libs/polyfill/include/mrdocs/; a unit test mirrors the header it tests"])

    def test_a_library_test_does_not_use_the_main_headers(self):
        gitrepo.write(self.repo, "libs/dom/tests/unit/Config.cpp", "")
        self.assertEqual(len(check_unit_test_mirror.check(self.repo, self._change())), 1)

    def test_library_drivers_and_non_unit_directories_are_not_judged(self):
        gitrepo.write(self.repo, "libs/dom/tests/unit/DomTestMain.cpp", "")
        gitrepo.write(self.repo, "libs/dom/tests/unit/UnitMain.cpp", "")
        gitrepo.write(self.repo, "libs/dom/tests/DomTestMain.cpp", "")
        gitrepo.write(self.repo, "libs/handlebars/tests/golden/Case.cpp", "")
        self.assertEqual(check_unit_test_mirror.check(self.repo, self._change()), [])

    def test_with_all_a_flat_library_test_is_not_found_and_a_unit_one_is(self):
        gitrepo.write(self.repo, "libs/dom/tests/Dom.cpp", "")
        gitrepo.write(self.repo, "libs/dom/tests/unit/Dom/Array.cpp", "")
        found = check_unit_test_mirror.find_tests(self.repo, None)
        self.assertIn("libs/dom/tests/unit/Dom/Array.cpp", found)
        self.assertNotIn("libs/dom/tests/Dom.cpp", found)

    def test_a_test_whose_header_the_branch_deletes_or_renames_is_reported(self):
        os.remove(self.repo / "include/mrdocs/Config.hpp")
        gitrepo.write(self.repo, "include/mrdocs/Renamed.hpp", "")
        os.remove(self.repo / "src/mrdocs/Extensions/ExtensionContext.hpp")
        found = check_unit_test_mirror.check(self.repo, self._change())
        self.assertEqual([m.split(":")[0] for m in found],
                         ["tests/unit/Config.cpp", "tests/unit/Extensions/ExtensionContext.cpp"])

    def test_a_test_whose_header_the_branch_edits_passes(self):
        gitrepo.write(self.repo, "include/mrdocs/Config.hpp", "edited\n")
        self.assertEqual(check_unit_test_mirror.check(self.repo, self._change()), [])

    def _run(self, *args):
        self.addCleanup(setattr, check_unit_test_mirror, "ROOT", check_unit_test_mirror.ROOT)
        check_unit_test_mirror.ROOT = self.repo
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_unit_test_mirror.main(list(args))
        return code, out.getvalue()

    def test_main_exit_codes(self):
        self.assertEqual(self._run("--base", "HEAD"), (0, ""))
        gitrepo.write(self.repo, "tests/unit/Extensions/ScriptContext.cpp", "")
        self.assertEqual(self._run("--base", "HEAD"), (1, MISSING + "\n"))

    def test_sarif_names_the_test(self):
        gitrepo.write(self.repo, "tests/unit/Extensions/ScriptContext.cpp", "")
        path = os.path.join(self.tmp, "out.sarif")
        code, out = self._run("--base", "HEAD", "--sarif", path)
        self.assertEqual(code, 1)
        with open(path, encoding="utf-8") as f:
            run = json.load(f)["runs"][0]
        (result,) = run["results"]
        self.assertEqual(result["ruleId"], "unit-test-mirror")
        self.assertEqual(result["message"]["text"], out.strip())
        self.assertEqual(
            result["locations"][0]["physicalLocation"]["artifactLocation"]["uri"],
            "tests/unit/Extensions/ScriptContext.cpp",
        )
        self.assertEqual(
            run["tool"]["driver"]["rules"],
            [{"id": "unit-test-mirror", "helpUri":
              "https://mrdocs.com/docs/mrdocs/develop/contribute/layout/tests.html#unit-test-mirror"}],
        )


if __name__ == "__main__":
    unittest.main()
