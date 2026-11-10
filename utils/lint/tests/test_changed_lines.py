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

"""Tests for utils/lint/changed_lines.py, run on temporary git repositories."""

import argparse
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
from tests import gitrepo


class ChangedLinesTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(self.tmp / "repo")
        gitrepo.write(self.repo, "src/kept.cpp", "int a;\n")

    def _base(self):
        gitrepo.commit_base(self.repo)
        return changed_lines.resolve_base(self.repo)[0]

    def test_base_is_the_merge_base_with_the_upstream_branch(self):
        gitrepo.git(self.repo, "add", ".")
        gitrepo.git(self.repo, "commit", "-q", "-m", "one")
        gitrepo.git(self.repo, "update-ref", "refs/remotes/upstream/develop", "HEAD")
        gitrepo.git(self.repo, "symbolic-ref", "refs/remotes/upstream/HEAD",
                    "refs/remotes/upstream/develop")
        tip = subprocess.run(["git", "rev-parse", "HEAD"], cwd=self.repo, text=True,
                             stdout=subprocess.PIPE).stdout.strip()
        gitrepo.write(self.repo, "new.hpp", "int b;\n")
        gitrepo.git(self.repo, "add", "new.hpp")
        gitrepo.git(self.repo, "commit", "-q", "-m", "two")
        base, tried = changed_lines.resolve_base(self.repo)
        self.assertEqual(base, tip)
        self.assertEqual(tried[0], "refs/remotes/upstream/develop")

    def test_origin_develop_is_the_fallback(self):
        gitrepo.git(self.repo, "add", ".")
        gitrepo.git(self.repo, "commit", "-q", "-m", "one")
        gitrepo.git(self.repo, "update-ref", "refs/remotes/origin/develop", "HEAD")
        base, tried = changed_lines.resolve_base(self.repo)
        self.assertIsNotNone(base)
        self.assertEqual(tried, ["upstream/develop", "origin/develop"])

    def test_a_path_with_spaces_or_non_ascii_letters_is_a_changed_file(self):
        gitrepo.write(self.repo, "docs/a b.adoc", "one\n")
        gitrepo.write(self.repo, "docs/\u00e9.adoc", "one\n")
        base = self._base()
        gitrepo.write(self.repo, "docs/a b.adoc", "one\ntwo\n")
        gitrepo.write(self.repo, "docs/\u00e9.adoc", "one\ntwo\n")
        change = changed_lines.Change(self.repo, base)
        self.assertIn("docs/a b.adoc", change.files())
        self.assertIn("docs/\u00e9.adoc", change.files())
        self.assertEqual(change.ranges("docs/\u00e9.adoc"), [(2, 2)])

    def test_base_override_and_missing_base(self):
        gitrepo.git(self.repo, "add", ".")
        gitrepo.git(self.repo, "commit", "-q", "-m", "one")
        gitrepo.git(self.repo, "branch", "other")
        self.assertIsNotNone(changed_lines.resolve_base(self.repo, "other")[0])
        base, tried = changed_lines.resolve_base(self.repo)
        self.assertIsNone(base)
        self.assertEqual(tried, ["upstream/develop", "origin/develop"])
        self.assertIsNone(changed_lines.resolve_base(self.repo, "nope")[0])

    def test_files_hold_committed_edited_and_untracked_files_but_not_ignored_or_deleted(self):
        gitrepo.write(self.repo, ".gitignore", "ignored.cpp\n")
        gitrepo.write(self.repo, "gone.cpp", "int g;\n")
        base = self._base()
        gitrepo.write(self.repo, "src/committed.hpp", "x\n")
        gitrepo.git(self.repo, "add", ".")
        gitrepo.git(self.repo, "commit", "-q", "-m", "two")
        gitrepo.write(self.repo, "src/kept.cpp", "int a2;\n")
        gitrepo.write(self.repo, "src/fresh.cpp", "x\n")
        gitrepo.write(self.repo, "ignored.cpp", "x\n")
        (self.repo / "gone.cpp").unlink()
        change = changed_lines.Change(self.repo, base)
        self.assertEqual(change.files(), ["src/committed.hpp", "src/fresh.cpp", "src/kept.cpp"])

    def test_files_ignore_a_forced_diff_color(self):
        base = self._base()
        gitrepo.git(self.repo, "config", "color.ui", "always")
        gitrepo.write(self.repo, "src/kept.cpp", "int a;\nint b;\n")
        change = changed_lines.Change(self.repo, base)
        self.assertEqual(change.ranges("src/kept.cpp"), [(2, 2)])
        self.assertEqual(change.files(), ["src/kept.cpp"])

    def test_lines_are_the_added_and_modified_lines(self):
        base = self._base()
        gitrepo.write(self.repo, "src/kept.cpp", "int a;\nint b;\nint c;\n")
        change = changed_lines.Change(self.repo, base)
        self.assertEqual(change.lines("src/kept.cpp"), {2, 3})

    def test_an_untracked_file_is_all_new(self):
        base = self._base()
        gitrepo.write(self.repo, "fresh.adoc", "a\nb\nc\n")
        change = changed_lines.Change(self.repo, base)
        self.assertEqual(change.ranges("fresh.adoc"), [(1, 3)])

    def _renamed(self, edit):
        gitrepo.write(self.repo, "src/old.cpp", "int a;\nint b;\nint c;\n")
        base = self._base()
        (self.repo / "src" / "sub").mkdir()
        (self.repo / "src" / "old.cpp").rename(self.repo / "src" / "sub" / "new.cpp")
        if edit:
            gitrepo.write(self.repo, "src/sub/new.cpp", "int a;\nint b2;\nint c;\n")
        gitrepo.git(self.repo, "add", ".")
        gitrepo.git(self.repo, "commit", "-q", "-m", "three")
        return changed_lines.Change(self.repo, base)

    def test_a_pure_rename_has_no_changed_lines(self):
        self.assertEqual(self._renamed(False).ranges("src/sub/new.cpp"), [])

    def test_a_renamed_file_counts_only_its_edited_lines(self):
        self.assertEqual(self._renamed(True).ranges("src/sub/new.cpp"), [(2, 2)])

    def test_options_and_from_args(self):
        parser = argparse.ArgumentParser()
        changed_lines.add_arguments(parser)
        base = self._base()
        change = changed_lines.from_args(parser.parse_args([]), self.repo)
        self.assertEqual(change.base, base)
        self.assertIsNone(changed_lines.from_args(parser.parse_args(["--all"]), self.repo))
        self.assertEqual(changed_lines.from_args(parser.parse_args(["--base", "HEAD"]), self.repo).base, base)
        with self.assertRaises(changed_lines.NoBase) as raised:
            changed_lines.from_args(parser.parse_args(["--base", "nope"]), self.repo)
        self.assertEqual(raised.exception.tried, ["nope"])

    def test_a_renamed_or_added_or_untracked_file_has_a_new_path(self):
        change = self._renamed(True)
        self.assertTrue(change.is_new_path("src/sub/new.cpp"))
        self.assertFalse(change.is_new_path("src/kept.cpp"))
        gitrepo.write(self.repo, "fresh.cpp", "int a;\n")
        self.assertTrue(changed_lines.Change(self.repo, change.base).is_new_path("fresh.cpp"))

    def test_lines_are_split_on_newlines_only(self):
        text = "a\x0cb\nc\x85\u2028\r\nd\re"
        self.assertEqual(changed_lines.split_lines(text), ["a\x0cb", "c\x85\u2028\r", "d\re"])
        self.assertEqual(changed_lines.split_lines(text, keepends=True), ["a\x0cb\n", "c\x85\u2028\r\n", "d\re"])
        self.assertEqual(changed_lines.split_lines("a\n"), ["a"])
        self.assertEqual(changed_lines.split_lines(""), [])

    def test_an_untracked_file_counts_its_lines_as_git_does(self):
        base = self._base()
        gitrepo.write(self.repo, "fresh.cpp", "a\x0cb\nc\u2028d\n")
        self.assertEqual(changed_lines.Change(self.repo, base).ranges("fresh.cpp"), [(1, 2)])

    def test_code_metrics_numbers_lines_as_git_does(self):
        import code_metrics
        kept = code_metrics.code_lines("// c\nint a; // x\x0cy\nint b;\n")
        self.assertEqual([index for _, index in kept], [1, 2])

    def test_a_moved_line_is_blamed_where_it_left(self):
        self.assertEqual(changed_lines.blamed_lines(
            ["#include <b>", "#include <d>", "#include <a>"],
            ["#include <a>", "#include <b>", "#include <d>"], None), [3])
        self.assertEqual(changed_lines.blamed_lines(
            ["b", "c", "d", "a"], ["a", "b", "c", "d"], {2}), [4])
        self.assertEqual(changed_lines.blamed_lines(
            ["b", "c", "d", "a"], ["a", "b", "c", "d"], {4}), [4])

    def test_a_changed_line_is_blamed_on_itself_and_an_added_line_on_its_successor(self):
        self.assertEqual(changed_lines.blamed_lines(["a", "b", "c"], ["a", "B", "c"], None), [2])
        self.assertEqual(changed_lines.blamed_lines(["a", "c"], ["a", "b", "c"], None), [2])
        self.assertEqual(changed_lines.blamed_lines(["a", "b"], ["a", "b"], None), [])


if __name__ == "__main__":
    unittest.main()
