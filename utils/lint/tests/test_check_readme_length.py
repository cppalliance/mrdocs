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

"""Tests for utils/lint/check_readme_length.py, run on scratch trees."""

import contextlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import changed_lines
import check_readme_length
from tests import gitrepo


def words(n):
    return " ".join(["word"] * n) + "\n"


class ReadmeLengthTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def _readme(self, directory, text):
        path = os.path.join(self.tmp, directory)
        os.makedirs(path, exist_ok=True)
        with open(os.path.join(path, "README.md"), "w") as f:
            f.write(text)

    def _check(self):
        return check_readme_length.check(Path(self.tmp))

    def test_under_cap_passes(self):
        self._readme("a", words(check_readme_length.MAX_WORDS))
        self.assertEqual(self._check(), [])

    def test_over_cap_is_reported(self):
        self._readme("a/b", words(check_readme_length.MAX_WORDS + 1))
        problems = self._check()
        self.assertEqual(len(problems), 1)
        self.assertTrue(
            problems[0].startswith("a/b/README.md: 181 words, the cap is 180")
        )

    def test_code_block_is_excluded(self):
        text = words(10) + "```\n" + words(500) + "```\n" + words(10)
        self._readme("a", text)
        self.assertEqual(check_readme_length.count_words(text), 20)
        self.assertEqual(self._check(), [])

    def test_skipped_directories(self):
        self._readme("third-party/lib", words(1000))
        self._readme("build/out", words(1000))
        self.assertEqual(self._check(), [])

    def test_git_lists_only_tracked_files(self):
        subprocess.run(["git", "init", "-q"], cwd=self.tmp, check=True)
        self._readme("tracked", words(500))
        self._readme("ignored", words(500))
        self._readme("ok", words(5))
        subprocess.run(
            ["git", "add", "tracked/README.md", "ok/README.md"],
            cwd=self.tmp, check=True,
        )
        problems = self._check()
        self.assertEqual(len(problems), 1)
        self.assertTrue(problems[0].startswith("tracked/README.md: "))

    def test_main_exit_codes(self):
        self.addCleanup(setattr, check_readme_length, "ROOT", check_readme_length.ROOT)
        check_readme_length.ROOT = Path(self.tmp)
        self._readme("a", words(5))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_readme_length.main(["--all"]), 0)
            self._readme("b", words(500))
            self.assertEqual(check_readme_length.main(["--all"]), 1)

    def _sarif_run(self, sarif_path):
        self.addCleanup(setattr, check_readme_length, "ROOT", check_readme_length.ROOT)
        check_readme_length.ROOT = Path(self.tmp)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_readme_length.main(["--all", "--sarif", sarif_path])
        with open(sarif_path, encoding="utf-8") as f:
            return code, out.getvalue(), json.load(f)["runs"][0]

    def test_sarif_of_a_passing_run_has_no_results(self):
        self._readme("a", words(5))
        code, out, run = self._sarif_run(os.path.join(self.tmp, "ok.sarif"))
        self.assertEqual((code, out), (0, ""))
        self.assertEqual(run["results"], [])
        self.assertEqual(
            run["tool"]["driver"]["rules"],
            [{"id": "readme-length", "helpUri":
              "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/documentation.html#readme-length"}],
        )

    def test_sarif_of_a_failing_run_names_the_readme(self):
        self._readme("a", words(5))
        self._readme("b", words(500))
        code, out, run = self._sarif_run(os.path.join(self.tmp, "bad.sarif"))
        self.assertEqual(code, 1)
        (result,) = run["results"]
        self.assertEqual(result["ruleId"], "readme-length")
        self.assertEqual(result["level"], "error")
        self.assertEqual(result["message"]["text"], out.strip())
        self.assertEqual(
            result["locations"][0]["physicalLocation"]["artifactLocation"]["uri"],
            "b/README.md",
        )


class IncrementalReadmeLengthTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(os.path.join(self.tmp, "repo"))
        self.long = "word " * (check_readme_length.MAX_WORDS + 1)
        gitrepo.write(self.repo, "old/README.md", self.long)
        gitrepo.write(self.repo, "edited/README.md", "short\n")
        gitrepo.commit_base(self.repo)

    def _names(self, change):
        return [rel for rel, _ in check_readme_length.find_problems(self.repo, change)]

    def _change(self):
        return changed_lines.Change(self.repo, changed_lines.resolve_base(self.repo)[0])

    def test_a_long_readme_the_branch_did_not_touch_passes(self):
        self.assertEqual(self._names(self._change()), [])
        self.assertEqual(self._names(None), ["old/README.md"])

    def test_an_edited_and_an_untracked_readme_are_judged(self):
        gitrepo.write(self.repo, "edited/README.md", self.long)
        gitrepo.write(self.repo, "fresh/README.md", self.long)
        gitrepo.write(self.repo, "build/README.md", self.long)
        gitrepo.write(self.repo, "fresh/NOTES.md", self.long)
        self.assertEqual(self._names(self._change()), ["edited/README.md", "fresh/README.md"])


if __name__ == "__main__":
    unittest.main()
