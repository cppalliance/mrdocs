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

"""Tests for utils/lint/check_layout.py, run on scratch repositories."""

import contextlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import check_layout
from tests import gitrepo

DIRS = ("docs/", "include/", "libs/", "tools/")
FILES = ("CMakeLists.txt", "README.md")
TOLERATED = (".github/",)


def page(dirs=DIRS, files=FILES, tolerated=TOLERATED):
    def table(anchor, entries, xref=False):
        rows = ""
        for entry in entries:
            cell = "xref:contribute/layout/%s.adoc[`%s`]" % (entry.rstrip("/"), entry) if xref else "`%s`" % entry
            rows += "|%s\n|role\n\n" % cell
        return "[#%s,cols=\"2,5\"]\n|===\n|Entry |Role\n\n%s|===\n\n" % (anchor, rows)
    return ("= Tour\n\n" + table("layout-allow-list", dirs, True)
            + table("layout-allow-files", files) + table("layout-tolerated", tolerated))


class LayoutCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        subprocess.run(["git", "init", "-q", self.tmp], check=True)
        for name, value in (("ALLOWED_DIRS", DIRS), ("ALLOWED_FILES", FILES), ("TOLERATED", TOLERATED)):
            patcher = mock.patch.object(check_layout, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        self._write(check_layout.PAGE, page())
        self._write(check_layout.LAYOUT_DIR + "/docs.adoc", "= docs\n")
        self._write(check_layout.LAYOUT_DIR + "/include.adoc", "= include\n")
        self._write(check_layout.LAYOUT_DIR + "/libs.adoc", "= libs\n")
        self._write(check_layout.LAYOUT_DIR + "/tools.adoc", "= tools\n")
        self._write("include/a.hpp", "\n")
        self._write("README.md", "\n")
        self._write("CMakeLists.txt", "\n")
        self._write("libs/CMakeLists.txt", "\n")
        self._write("tools/CMakeLists.txt", "\n")
        self._write("libs/lib/CMakeLists.txt", "\n")
        self._write("libs/lib/include/a.hpp", "\n")

    def _write(self, rel, text):
        path = Path(self.tmp, rel)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def _run(self, *extra):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            scope = [] if "--base" in extra else ["--all"]
            code = check_layout.main(["--repo", self.tmp, *scope, *extra])
        return code, out.getvalue()

    def _sarif(self):
        with open(os.path.join(self.tmp, "out.sarif"), encoding="utf-8") as f:
            return json.load(f)["runs"][0]


class CheckLayoutTests(LayoutCase):
    def test_passing_run_writes_no_results(self):
        sarif_path = os.path.join(self.tmp, "out.sarif")
        code, out = self._run("--sarif", sarif_path)
        self.assertEqual(code, 0)
        self.assertEqual(out, "layout: ok (6 entries)\n")
        run = self._sarif()
        self.assertEqual(run["results"], [])
        self.assertEqual(
            run["tool"]["driver"]["rules"][0]["helpUri"],
            "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/layout.html#layout",
        )

    def test_failing_run_writes_one_result_per_violation(self):
        self._write("stray.txt", "\n")
        self._write("include/README.md", "\n")
        sarif_path = os.path.join(self.tmp, "out.sarif")
        code, out = self._run("--sarif", sarif_path)
        self.assertEqual(code, 1)
        lines = out.splitlines()
        self.assertEqual(len(lines), 2)
        results = self._sarif()["results"]
        self.assertEqual(len(results), 2)
        self.assertEqual({r["ruleId"] for r in results}, {"layout"})
        self.assertEqual({r["level"] for r in results}, {"error"})
        self.assertEqual(
            sorted(r["locations"][0]["physicalLocation"]["artifactLocation"]["uri"]
                   for r in results),
            ["include/README.md", "stray.txt"],
        )
        self.assertEqual(
            sorted(r["message"]["text"] for r in results),
            sorted(line[len("layout: "):] for line in lines),
        )

    def test_tracked_tolerated_entry_passes(self):
        self._write(".github/workflows/ci.yml", "\n")
        code, out = self._run()
        self.assertEqual(code, 0, out)

    def test_ignored_tolerated_entries_need_no_row(self):
        self._write(".gitignore", "build/\n")
        self._write("build/x.txt", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn(".gitignore: tracked at the top level but not in the layout lists", out)
        self.assertNotIn("build", out)

    def test_table_cell_forms(self):
        text = page()
        self.assertEqual(check_layout.parse_table(text, "layout-allow-list"), list(DIRS))
        self.assertEqual(check_layout.parse_table(text, "layout-allow-files"), list(FILES))
        self.assertEqual(check_layout.parse_table(text, "layout-tolerated"), list(TOLERATED))
        self.assertIsNone(check_layout.parse_table(text, "layout-missing"))

    def test_table_row_the_lists_lack_fails(self):
        self._write(check_layout.PAGE, page(files=FILES + ("extra.txt",)))
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertEqual(out.count("\n"), 1)
        self.assertIn("extra.txt: in table 'layout-allow-files' on %s but missing from ALLOWED_FILES" % check_layout.PAGE, out)

    def test_list_entry_the_table_lacks_fails(self):
        self._write(check_layout.PAGE, page(dirs=("docs/", "include/", "tools/")))
        sarif_path = os.path.join(self.tmp, "out.sarif")
        code, out = self._run("--sarif", sarif_path)
        self.assertEqual(code, 1)
        self.assertIn("libs/: in ALLOWED_DIRS but missing from table 'layout-allow-list' on %s" % check_layout.PAGE, out)
        results = self._sarif()["results"]
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["ruleId"], "layout")

    def test_tolerated_table_is_compared(self):
        self._write(check_layout.PAGE, page(tolerated=()))
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn(".github/: in TOLERATED but missing from table 'layout-tolerated'", out)

    def test_missing_table_and_missing_page_fail(self):
        self._write(check_layout.PAGE, page().replace("layout-tolerated", "other"))
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("table 'layout-tolerated' not found", out)
        os.remove(os.path.join(self.tmp, check_layout.PAGE))
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("page not found", out)

    def test_table_row_listed_twice_fails(self):
        self._write(check_layout.PAGE, page(files=FILES + ("README.md",)))
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("README.md: listed more than once in table 'layout-allow-files'", out)

    def test_table_row_without_code_span_fails(self):
        self._write(check_layout.PAGE, page().replace("|`README.md`", "|README.md"))
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("table 'layout-allow-files' has a row without a code span", out)

    def test_entry_in_two_lists_fails(self):
        with mock.patch.object(check_layout, "TOLERATED", TOLERATED + ("docs/",)):
            self._write(check_layout.PAGE, page(tolerated=TOLERATED + ("docs/",)))
            code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("docs/: listed more than once in the layout lists", out)

    def test_library_with_an_extra_top_level_entry_fails(self):
        self._write("libs/lib/scripts/run.py", "\n")
        self._write("libs/lib/notes.txt", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("libs/lib/scripts/: not a directory the root layout allows in a library", out)
        self.assertIn("libs/lib/notes.txt: not a file the root layout allows in a library", out)

    def test_nested_libs_is_checked(self):
        self._write("libs/lib/libs/inner/CMakeLists.txt", "\n")
        self._write("libs/lib/libs/inner/include/a.hpp", "\n")
        code, out = self._run()
        self.assertEqual(code, 0, out)
        self._write("libs/lib/libs/inner/extras/x.txt", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("libs/lib/libs/inner/extras/: not a directory the root layout allows", out)

    def test_nested_libs_two_levels_deep_is_checked(self):
        self._write("libs/lib/libs/inner/libs/leaf/stray.txt", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("libs/lib/libs/inner/libs/leaf/stray.txt: not a file", out)

    def test_tool_root_passes(self):
        self._write("tools/mrdocs/CMakeLists.txt", "\n")
        self._write("tools/mrdocs/include/a.hpp", "\n")
        code, out = self._run()
        self.assertEqual(code, 0, out)

    def test_tool_root_with_a_stray_directory_fails(self):
        self._write("tools/mrdocs/CMakeLists.txt", "\n")
        self._write("tools/mrdocs/scripts/run.py", "\n")
        self._write("tools/mrdocs/README.md", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("tools/mrdocs/scripts/: not a directory the root layout allows in a C++ tool", out)
        self.assertIn("tools/mrdocs/README.md: not allowed, a C++ tool has no README.md at its top", out)

    def test_library_inside_a_tool_is_checked(self):
        self._write("tools/mrdocs/CMakeLists.txt", "\n")
        self._write("tools/mrdocs/libs/inner/stray.txt", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("tools/mrdocs/libs/inner/stray.txt: not a file the root layout allows in a library", out)

    def test_tool_outside_the_list_is_not_checked(self):
        self._write("tools/other/scripts/run.py", "\n")
        code, out = self._run()
        self.assertEqual(code, 0, out)

    def test_readme_at_a_library_top_fails(self):
        self._write("libs/lib/README.md", "\n")
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("libs/lib/README.md: not allowed, a library has no README.md at its top", out)

    def test_output_and_status_do_not_depend_on_the_option(self):
        self._write("stray.txt", "\n")
        plain = self._run()
        with_sarif = self._run("--sarif", os.path.join(self.tmp, "out.sarif"))
        self.assertEqual(plain, with_sarif)


class IncrementalLayoutTests(LayoutCase):
    """The branch answers for the entries it adds or removes, never for the base."""

    def _base(self):
        gitrepo.commit_base(self.tmp)

    def _incremental(self, *extra):
        return self._run("--base", "HEAD", *extra)

    def test_a_clean_branch_passes(self):
        self._base()
        self._write("include/b.hpp", "\n")
        code, out = self._incremental()
        self.assertEqual(code, 0, out)

    def test_a_problem_the_base_already_had_does_not_fail_the_branch(self):
        self._write("scripts/run.sh", "\n")
        self._base()
        self._write("include/b.hpp", "\n")
        self.assertEqual(self._incremental()[0], 0)
        code, out = self._run()
        self.assertEqual(code, 1)

    def test_a_top_level_entry_the_branch_adds_fails_on_that_entry_alone(self):
        self._write("scripts/run.sh", "\n")
        self._base()
        self._write("scripts2/run.sh", "\n")
        code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("scripts2/: tracked at the top level", out)
        self.assertNotIn("scripts/:", out)

    def test_a_required_entry_the_branch_removes_fails(self):
        self._base()
        shutil.rmtree(Path(self.tmp, "tools"))
        code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("tools/: allowed directory is missing from the tree", out)

    def test_a_library_directory_the_branch_adds_fails(self):
        self._base()
        self._write("libs/lib/stray/a.hpp", "\n")
        code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("libs/lib/stray/: not a directory the root layout allows", out)

    def test_a_layout_page_the_branch_adds_for_no_directory_fails(self):
        self._base()
        self._write(check_layout.LAYOUT_DIR + "/extra.adoc", "= extra\n")
        code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("extra.adoc: page for a name that is not an allowed directory", out)

    def test_a_layout_page_the_branch_removes_fails(self):
        self._base()
        os.remove(Path(self.tmp, check_layout.LAYOUT_DIR, "tools.adoc"))
        code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("tools.adoc: missing page for the allowed directory tools/", out)

    def test_the_tables_are_compared_only_when_the_branch_edits_the_page(self):
        self._write(check_layout.PAGE, page(dirs=DIRS[:-1]))
        self._base()
        self._write("include/b.hpp", "\n")
        self.assertEqual(self._incremental()[0], 0)
        self._write(check_layout.PAGE, page(dirs=DIRS[:-1]) + "\nedited\n")
        code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("tools/: in ALLOWED_DIRS but missing from table 'layout-allow-list'", out)

    def test_the_sarif_holds_the_branch_findings_only(self):
        self._write("scripts/run.sh", "\n")
        self._base()
        self._write("scripts2/run.sh", "\n")
        target = os.path.join(self.tmp, "out.sarif")
        self._incremental("--sarif", target)
        with open(target, encoding="utf-8") as f:
            results = json.load(f)["runs"][0]["results"]
        self.assertEqual([r["locations"][0]["physicalLocation"]["artifactLocation"]["uri"] for r in results],
                         ["scripts2"])

    def _base_with_lists(self):
        text = ("ALLOWED_DIRS = %r\nALLOWED_FILES = %r\nTOLERATED = %r\n" % (DIRS, FILES, TOLERATED))
        self._write(check_layout.SELF, text)
        self._base()

    def test_a_branch_that_drops_a_directory_from_the_lists_fails(self):
        self._base_with_lists()
        self._write(check_layout.SELF, "ALLOWED_DIRS = %r\nALLOWED_FILES = %r\nTOLERATED = %r\n"
                    % (DIRS[:-1], FILES, TOLERATED))
        with mock.patch.object(check_layout, "ALLOWED_DIRS", DIRS[:-1]):
            self._write(check_layout.PAGE, page(dirs=DIRS[:-1]))
            os.remove(Path(self.tmp, check_layout.LAYOUT_DIR, "tools.adoc"))
            code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("tools/: tracked at the top level but not in the layout lists", out)

    def test_a_branch_that_allows_a_directory_nobody_has_fails(self):
        self._base_with_lists()
        with mock.patch.object(check_layout, "ALLOWED_DIRS", DIRS + ("notes/",)):
            code, out = self._incremental()
        self.assertEqual(code, 1)
        self.assertIn("notes/: allowed directory is missing from the tree", out)

    def test_the_lists_of_the_base_are_read_without_running_the_file(self):
        self._write(check_layout.SELF, "ALLOWED_DIRS = ('a/',)\nALLOWED_FILES = ()\n"
                    "TOLERATED = ()\nraise SystemExit(5)\n")
        self._base()
        change = check_layout.changed_lines.Change(self.tmp, "HEAD")
        self.assertEqual(check_layout.lists_at(change), (("a/",), (), ()))

    def test_no_base_without_all_fails(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_layout.main(["--repo", self.tmp])
        self.assertEqual(code, 1)
        self.assertIn("layout: no base found", out.getvalue())


if __name__ == "__main__":
    unittest.main()
