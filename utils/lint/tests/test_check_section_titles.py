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

"""Tests for utils/lint/check_section_titles.py, run on scratch trees."""

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
import check_section_titles
from tests import gitrepo


class SectionTitleTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def _page(self, name, text):
        path = os.path.join(self.tmp, "docs/modules/ROOT/pages", name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    def _check(self):
        return check_section_titles.check(Path(self.tmp))

    def test_a_what_title_is_flagged(self):
        self._page("a/b.adoc", "= Page\n\nText.\n\n== What goes elsewhere\n\nText.\n")
        (problem,) = self._check()
        self.assertEqual(
            problem,
            'docs/modules/ROOT/pages/a/b.adoc:5: section title "What goes '
            'elsewhere" opens with "What"; name the concept the section '
            'discusses instead, such as "Content that belongs elsewhere"')

    def test_where_when_and_deeper_levels_are_flagged(self):
        self._page("a.adoc", "= Page\n\n=== Where it lives\n\n====== when to use it\n")
        self.assertEqual(len(self._check()), 2)

    def test_a_page_title_is_not_flagged(self):
        self._page("a.adoc", "= What this project is\n\n== Overview\n")
        self.assertEqual(self._check(), [])

    def test_titles_inside_blocks_are_not_flagged(self):
        self._page("a.adoc", "= Page\n\n----\n== What in code\n----\n\n"
                   "....\n== Where in literal\n....\n\n"
                   "////\n== When in comment\n////\n\n"
                   "++++\n== What in passthrough\n++++\n\n"
                   "// == What in a line comment\n\n== Overview\n")
        self.assertEqual(self._check(), [])

    def test_longer_fences_hide_their_titles(self):
        self._page("a.adoc", "= Page\n\n//////\n== What in comment\n//////\n\n"
                   "+++++\n== When in passthrough\n+++++\n\n------\n----\n"
                   "== Where in code\n------\n")
        self.assertEqual(self._check(), [])

    def test_titles_behind_an_anchor_or_emphasis_are_flagged(self):
        self._page("a.adoc", "= Page\n\n== [[where]]Where it lives\n\n"
                   "== _What_ goes elsewhere\n\n== *When* to use it\n")
        self.assertEqual(len(self._check()), 3)

    def test_a_title_after_a_block_is_flagged(self):
        self._page("a.adoc", "= Page\n\n----\ncode\n----\n\n== When it runs\n")
        (problem,) = self._check()
        self.assertIn("a.adoc:7:", problem)

    def test_words_that_only_start_with_what_are_not_flagged(self):
        self._page("a.adoc", "= Page\n\n== Whatever happens\n\n== Whenever\n\n"
                   "== Wherein\n\n== Somewhat\n")
        self.assertEqual(self._check(), [])

    def test_how_why_which_who_and_in_what_are_flagged(self):
        self._page("a.adoc", "= Page\n\n== How it runs\n\n== Why generate\n\n"
                   "== Which suites exist\n\n== Who owns it\n\n"
                   "== In what order to include headers\n\n== in WHAT order\n")
        problems = self._check()
        self.assertEqual(len(problems), 6)
        self.assertIn('opens with "In what"', problems[4])
        self.assertIn('opens with "How"', problems[0])

    def test_a_title_that_ends_with_a_question_mark_is_flagged(self):
        self._page("a.adoc", "= Page\n\n== Include order?\n\n== Is it ready *?*\n")
        problems = self._check()
        self.assertEqual(len(problems), 2)
        self.assertTrue(all('ends with "?"' in p for p in problems))

    def test_a_page_title_with_how_or_a_question_mark_is_exempt(self):
        self._page("a.adoc", "= How to Contribute a Feature\n\n== Overview\n")
        self._page("b.adoc", "= Ready?\n\n== Overview\n")
        self.assertEqual(self._check(), [])

    def test_words_that_only_start_with_the_question_words_are_not_flagged(self):
        self._page("a.adoc", "= Page\n\n== Whole tree\n\n== Whichever applies\n\n"
                   "== Whylike\n\n== Howitzer\n\n== How-to guides\n\n== Inwhat\n\n== In whatever\n")
        self.assertEqual(self._check(), [])

    def test_only_adoc_pages_count(self):
        self._page("a.txt", "== What\n")
        self.assertEqual(self._check(), [])

    def test_an_xref_to_a_generated_id_is_flagged(self):
        self._page("a.adoc", "= Page\n\nSee xref:b.adoc#_build_the_docs[Docs].\n")
        (problem,) = self._check()
        self.assertEqual(
            problem,
            'docs/modules/ROOT/pages/a.adoc:3: link to the generated id '
            '"_build_the_docs"; give the target section an explicit anchor, '
            'such as "[#build-the-docs]" above its title, and link to that')

    def test_a_same_page_xref_and_both_angle_forms_are_flagged(self):
        self._page("a.adoc", "= Page\n\nxref:#_one[One], <<_two>> and "
                             "<<b.adoc#_three,Three>>.\n")
        self.assertEqual(len(self._check()), 3)

    def test_links_to_explicit_anchors_are_not_flagged(self):
        self._page("a.adoc", "= Page\n\nxref:b.adoc#build-docs[Docs], <<build-docs>>, "
                             "<<b.adoc#build-docs,Docs>>, xref:b.adoc[B] and "
                             "xref:b.adoc#a_b[Snake].\n")
        self.assertEqual(self._check(), [])

    def test_links_inside_blocks_and_comments_are_not_flagged(self):
        self._page("a.adoc", "= Page\n\n----\nxref:b.adoc#_x[X]\n----\n"
                             "// xref:b.adoc#_y[Y]\n")
        self.assertEqual(self._check(), [])

    def test_main_exit_codes(self):
        self.addCleanup(setattr, check_section_titles, "ROOT", check_section_titles.ROOT)
        check_section_titles.ROOT = Path(self.tmp)
        self._page("a.adoc", "= Page\n\n== Overview\n")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(check_section_titles.main(["--all"]), 0)
            self._page("b.adoc", "= Page\n\n== What is it\n")
            self.assertEqual(check_section_titles.main(["--all"]), 1)

    def _sarif_run(self, sarif_path):
        self.addCleanup(setattr, check_section_titles, "ROOT", check_section_titles.ROOT)
        check_section_titles.ROOT = Path(self.tmp)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_section_titles.main(["--all", "--sarif", sarif_path])
        with open(sarif_path, encoding="utf-8") as f:
            return code, out.getvalue(), json.load(f)["runs"][0]

    def test_sarif_of_a_passing_run_has_no_results(self):
        self._page("a.adoc", "= Page\n\n== Overview\n")
        code, out, run = self._sarif_run(os.path.join(self.tmp, "ok.sarif"))
        self.assertEqual((code, out), (0, ""))
        self.assertEqual(run["results"], [])
        self.assertEqual(
            run["tool"]["driver"]["rules"],
            [{"id": "section-title", "helpUri":
              "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/documentation.html#section-title"}],
        )

    def test_sarif_of_a_failing_run_names_the_page_and_line(self):
        self._page("b.adoc", "= Page\n\n== Where it goes\n")
        code, out, run = self._sarif_run(os.path.join(self.tmp, "bad.sarif"))
        self.assertEqual(code, 1)
        (result,) = run["results"]
        self.assertEqual(result["ruleId"], "section-title")
        self.assertEqual(result["level"], "error")
        self.assertEqual(result["message"]["text"], out.strip())
        physical = result["locations"][0]["physicalLocation"]
        self.assertEqual(physical["artifactLocation"]["uri"],
                         "docs/modules/ROOT/pages/b.adoc")
        self.assertEqual(physical["region"]["startLine"], 3)


class IncrementalSectionTitleTests(unittest.TestCase):
    PAGE = "docs/modules/ROOT/pages/a.adoc"

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(os.path.join(self.tmp, "repo"))
        gitrepo.write(self.repo, self.PAGE, "= Page\n\n== What stays old\n\nText.\n")
        gitrepo.write(self.repo, "docs/modules/ROOT/pages/b.adoc", "= B\n\n== Why old\n")
        gitrepo.commit_base(self.repo)

    def _lines(self, change):
        return [(rel, line) for rel, line, _ in check_section_titles.find_problems(self.repo, change)]

    def _change(self):
        return changed_lines.Change(self.repo, changed_lines.resolve_base(self.repo)[0])

    def test_only_titles_on_changed_lines_are_judged(self):
        gitrepo.write(self.repo, self.PAGE,
                      "= Page\n\n== What stays old\n\nText.\n\n== How it is new\n")
        self.assertEqual(self._lines(self._change()), [(self.PAGE, 7)])
        self.assertEqual(self._lines(None), [
            (self.PAGE, 3), (self.PAGE, 7), ("docs/modules/ROOT/pages/b.adoc", 3)])

    def test_an_untouched_page_and_a_clean_change_pass(self):
        gitrepo.write(self.repo, self.PAGE, "= Page\n\n== What stays old\n\nText changed.\n")
        self.assertEqual(self._lines(self._change()), [])

    def test_a_new_page_is_all_new(self):
        gitrepo.write(self.repo, "docs/modules/ROOT/pages/c.adoc", "= C\n\n== Which one?\n")
        self.assertEqual(self._lines(self._change()),
                         [("docs/modules/ROOT/pages/c.adoc", 3)])

    def test_a_retitled_section_is_judged_on_its_new_line(self):
        gitrepo.write(self.repo, self.PAGE, "= Page\n\n== Where it moved\n\nText.\n")
        self.assertEqual(self._lines(self._change()), [(self.PAGE, 3)])

    def test_only_links_on_changed_lines_are_judged(self):
        gitrepo.write(self.repo, self.PAGE,
                      "= Page\n\nxref:b.adoc#_old[Old].\n\nxref:b.adoc#_new[New].\n")
        gitrepo.commit_base(self.repo)
        gitrepo.write(self.repo, self.PAGE,
                      "= Page\n\nxref:b.adoc#_old[Old].\n\nxref:b.adoc#_new[New] again.\n")
        self.assertEqual(self._lines(self._change()), [(self.PAGE, 5)])
        self.assertEqual(self._lines(None), [(self.PAGE, 3), (self.PAGE, 5), ("docs/modules/ROOT/pages/b.adoc", 3)])

    def test_main_takes_base_and_all(self):
        self.addCleanup(setattr, check_section_titles, "ROOT", check_section_titles.ROOT)
        check_section_titles.ROOT = self.repo
        with contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(check_section_titles.main([]), 0)
            self.assertEqual(check_section_titles.main(["--base", "HEAD"]), 0)
            self.assertEqual(check_section_titles.main(["--all"]), 1)
            gitrepo.git(self.repo, "update-ref", "-d", "refs/remotes/upstream/develop")
            self.assertEqual(check_section_titles.main([]), 1)
        self.assertIn("no base found, tried", out.getvalue())


if __name__ == "__main__":
    unittest.main()
