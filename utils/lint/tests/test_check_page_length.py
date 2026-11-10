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

"""Tests for utils/lint/check_page_length.py, run on scratch trees."""

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
import check_page_length
from tests import gitrepo


def words(n):
    return " ".join(["word"] * n) + "\n"


class PageLengthTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def _page(self, name, text):
        path = os.path.join(self.tmp, "docs/modules/ROOT/pages", name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    def _check(self):
        return check_page_length.check(Path(self.tmp))

    def test_under_cap_passes(self):
        self._page("a.adoc", words(check_page_length.MAX_WORDS))
        self.assertEqual(self._check(), [])

    def test_over_cap_is_reported(self):
        self._page("a/b.adoc", words(check_page_length.MAX_WORDS + 1))
        (problem,) = self._check()
        self.assertTrue(problem.startswith(
            "docs/modules/ROOT/pages/a/b.adoc: %d words, the cap is %d"
            % (check_page_length.MAX_WORDS + 1, check_page_length.MAX_WORDS)))

    def test_code_blocks_and_includes_are_excluded(self):
        text = (words(10) + "[source]\n----\n" + words(5000) + "----\n"
                + "....\n" + words(5000) + "....\n"
                + "include::example.adoc[]\n" + words(10)
                + "// " + words(50) + "////\n" + words(500) + "////\n"
                + "++++\n" + words(500) + "++++\n")
        self.assertEqual(check_page_length.count_words(text), 21)
        self._page("a.adoc", text)
        self.assertEqual(self._check(), [])

    def test_only_adoc_pages_count(self):
        self._page("a.txt", words(check_page_length.MAX_WORDS + 1))
        self.assertEqual(self._check(), [])

    def test_main_exit_codes(self):
        self.addCleanup(setattr, check_page_length, "ROOT", check_page_length.ROOT)
        check_page_length.ROOT = Path(self.tmp)
        self._page("a.adoc", words(5))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_page_length.main(["--all"]), 0)
            self._page("b.adoc", words(check_page_length.MAX_WORDS + 1))
            self.assertEqual(check_page_length.main(["--all"]), 1)

    def _sarif_run(self, sarif_path):
        self.addCleanup(setattr, check_page_length, "ROOT", check_page_length.ROOT)
        check_page_length.ROOT = Path(self.tmp)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_page_length.main(["--all", "--sarif", sarif_path])
        with open(sarif_path, encoding="utf-8") as f:
            return code, out.getvalue(), json.load(f)["runs"][0]

    def test_sarif_of_a_passing_run_has_no_results(self):
        self._page("a.adoc", words(5))
        code, out, run = self._sarif_run(os.path.join(self.tmp, "ok.sarif"))
        self.assertEqual((code, out), (0, ""))
        self.assertEqual(run["results"], [])
        self.assertEqual(
            run["tool"]["driver"]["rules"],
            [{"id": "page-length", "helpUri":
              "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/documentation.html#page-length"}],
        )

    def test_sarif_of_a_failing_run_names_the_page(self):
        self._page("a.adoc", words(5))
        self._page("b.adoc", words(check_page_length.MAX_WORDS + 1))
        code, out, run = self._sarif_run(os.path.join(self.tmp, "bad.sarif"))
        self.assertEqual(code, 1)
        (result,) = run["results"]
        self.assertEqual(result["ruleId"], "page-length")
        self.assertEqual(result["level"], "error")
        self.assertEqual(result["message"]["text"], out.strip())
        self.assertEqual(
            result["locations"][0]["physicalLocation"]["artifactLocation"]["uri"],
            "docs/modules/ROOT/pages/b.adoc",
        )


class IncrementalPageLengthTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(os.path.join(self.tmp, "repo"))
        self.pages = "docs/modules/ROOT/pages/"
        gitrepo.write(self.repo, self.pages + "old.adoc", words(check_page_length.MAX_WORDS + 1))
        gitrepo.write(self.repo, self.pages + "edited.adoc", "= Edited\n")
        gitrepo.commit_base(self.repo)

    def _change(self):
        return changed_lines.Change(self.repo, changed_lines.resolve_base(self.repo)[0])

    def _names(self, change):
        return [rel for rel, _ in check_page_length.find_problems(self.repo, change)]

    def test_a_long_page_the_branch_did_not_touch_passes(self):
        self.assertEqual(self._names(self._change()), [])
        self.assertEqual(self._names(None), [self.pages + "old.adoc"])

    def test_an_edited_and_an_untracked_page_are_judged(self):
        gitrepo.write(self.repo, self.pages + "edited.adoc", words(check_page_length.MAX_WORDS + 1))
        gitrepo.write(self.repo, self.pages + "fresh.adoc", words(check_page_length.MAX_WORDS + 1))
        self.assertEqual(self._names(self._change()),
                         [self.pages + "edited.adoc", self.pages + "fresh.adoc"])

    def test_main_takes_base_and_all(self):
        self.addCleanup(setattr, check_page_length, "ROOT", check_page_length.ROOT)
        check_page_length.ROOT = self.repo
        gitrepo.write(self.repo, self.pages + "fresh.adoc", words(check_page_length.MAX_WORDS + 1))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(check_page_length.main([]), 1)
            self.assertIn("fresh.adoc", out.getvalue())
            self.assertNotIn("old.adoc", out.getvalue())
            out.truncate(0)
            self.assertEqual(check_page_length.main(["--all"]), 1)
            self.assertIn("old.adoc", out.getvalue())
            gitrepo.git(self.repo, "update-ref", "-d", "refs/remotes/upstream/develop")
            out.truncate(0)
            out.seek(0)
            self.assertEqual(check_page_length.main([]), 1)
            self.assertIn("no base found", out.getvalue())
            self.assertEqual(check_page_length.main(["--base", "HEAD"]), 1)


if __name__ == "__main__":
    unittest.main()
