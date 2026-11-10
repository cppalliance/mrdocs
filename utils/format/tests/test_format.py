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

"""Tests for utils/format/format.py, run on scratch trees."""

import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "..", "lint"))

import changed_lines
import format as formatter
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "..", "lint", "tests"))

import gitrepo

REPO_ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))

BAD_GUARD = "#ifndef WRONG\n#define WRONG\nint x;\n#endif\n"
GOOD_GUARD = (
    "#ifndef MRDOCS_API_GOOD_HPP\n#define MRDOCS_API_GOOD_HPP\n"
    "int y;\n#endif // MRDOCS_API_GOOD_HPP\n"
)


def _run(argv, repo_root, sarif_path=None, change=None):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        paths = [os.path.abspath(p) for p in argv if not p.startswith("--")]
        code = formatter.run(paths, "--check" in argv, repo_root, sarif_path, change)
    return code, out.getvalue()


class FormatTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        shutil.copy(os.path.join(REPO_ROOT, ".clang-format"), self.tmp)
        self.hdr_dir = os.path.join(self.tmp, "include", "mrdocs")
        os.makedirs(self.hdr_dir)
        self.bad = os.path.join(self.hdr_dir, "Bad.hpp")
        self.good = os.path.join(self.hdr_dir, "Good.hpp")
        self._write(self.bad, BAD_GUARD)
        self._write(self.good, GOOD_GUARD)

    def _write(self, path, text):
        with open(path, "w", newline="") as f:
            f.write(text)

    def _read(self, path):
        with open(path, newline="") as f:
            return f.read()

    def test_check_reports_and_does_not_write(self):
        code, out = _run(["--check", self.bad], self.tmp)
        self.assertEqual(code, 1)
        self.assertIn("include/mrdocs/Bad.hpp:1: include guard WRONG would become MRDOCS_API_BAD_HPP (header-rules)", out)
        self.assertIn("include/mrdocs/Bad.hpp:2:", out)
        self.assertIn("include/mrdocs/Bad.hpp:4:", out)
        self.assertEqual(self._read(self.bad), BAD_GUARD)

    def test_check_clean_file_exits_zero(self):
        code, out = _run(["--check", self.good], self.tmp)
        self.assertEqual(code, 0)
        self.assertNotIn(".hpp:", out)

    def test_single_path_formats_only_that_file(self):
        code, _ = _run([self.bad], self.tmp)
        self.assertEqual(code, 0)
        self.assertIn("#ifndef MRDOCS_API_BAD_HPP", self._read(self.bad))
        self.assertEqual(self._read(self.good), GOOD_GUARD)

    def test_no_path_walks_default_roots(self):
        code, out = _run(["--check"], self.tmp)
        self.assertEqual(code, 1)
        self.assertIn("Bad.hpp:1:", out)
        self.assertNotIn("Good.hpp:", out)
        self.assertEqual(self._read(self.bad), BAD_GUARD)

    def test_directory_path(self):
        code, out = _run(["--check", self.hdr_dir], self.tmp)
        self.assertEqual(code, 1)
        self.assertIn("Bad.hpp", out)

    @unittest.skipIf(formatter.CLANG_FORMAT_EXE is None, "clang-format not found")
    def test_check_reports_clang_format_step(self):
        path = os.path.join(self.hdr_dir, "Fmt.hpp")
        text = ("#ifndef MRDOCS_API_FMT_HPP\n#define MRDOCS_API_FMT_HPP\n"
                "#include <vector>\n#include <algorithm>\n"
                "#endif // MRDOCS_API_FMT_HPP\n")
        self._write(path, text)
        code, out = _run(["--check", path], self.tmp)
        self.assertEqual(code, 1)
        self.assertIn("include/mrdocs/Fmt.hpp:3: clang-format would change this line (clang-format)", out)
        self.assertEqual(self._read(path), text)

    def test_a_missing_clang_format_is_a_warning_at_the_end_of_the_output(self):
        original = formatter.CLANG_FORMAT_EXE
        self.addCleanup(setattr, formatter, "CLANG_FORMAT_EXE", original)
        formatter.CLANG_FORMAT_EXE = None
        code, out = _run([self.good], self.tmp)
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines()[-1],
                         "warning: clang-format not found on PATH; formatting did not run "
                         "(install clang-format 21)")
        self.assertEqual(out.count("clang-format not found"), 1)

    def test_a_missing_clang_format_is_a_warning_result_in_the_sarif(self):
        original = formatter.CLANG_FORMAT_EXE
        self.addCleanup(setattr, formatter, "CLANG_FORMAT_EXE", original)
        formatter.CLANG_FORMAT_EXE = None
        target = os.path.join(self.tmp, "w.sarif")
        _run(["--check", self.good], self.tmp, target)
        (result,) = self._sarif(target)["results"]
        self.assertEqual(result["level"], "warning")
        self.assertIn("clang-format not found on PATH", result["message"]["text"])

    def test_a_present_clang_format_prints_no_warning(self):
        original = formatter.CLANG_FORMAT_EXE
        if original is None:
            self.skipTest("clang-format not found")
        _, out = _run([self.good], self.tmp)
        self.assertNotIn("warning:", out)

    def _sarif(self, path):
        with open(path, encoding="utf-8") as f:
            return json.load(f)["runs"][0]

    def test_sarif_of_a_failing_run_names_the_file(self):
        sarif_path = os.path.join(self.tmp, "bad.sarif")
        code, out = _run(["--check", self.bad], self.tmp, sarif_path)
        self.assertEqual(code, 1)
        results = self._sarif(sarif_path)["results"]
        self.assertEqual([r["locations"][0]["physicalLocation"]["region"]["startLine"] for r in results],
                         [1, 2, 4])
        result = results[0]
        self.assertEqual(result["ruleId"], "header-rules")
        self.assertEqual(result["level"], "error")
        self.assertEqual(result["message"]["text"], "include guard WRONG would become MRDOCS_API_BAD_HPP")
        self.assertEqual(
            result["locations"][0]["physicalLocation"]["artifactLocation"]["uri"],
            "include/mrdocs/Bad.hpp",
        )
        self.assertEqual(self._read(self.bad), BAD_GUARD)

    def test_sarif_of_a_passing_run_has_no_results(self):
        sarif_path = os.path.join(self.tmp, "ok.sarif")
        code, _ = _run(["--check", self.good], self.tmp, sarif_path)
        self.assertEqual(code, 0)
        run = self._sarif(sarif_path)
        self.assertEqual(run["results"], [])
        self.assertEqual(
            [(r["id"], r["helpUri"]) for r in run["tool"]["driver"]["rules"]],
            [("header-rules", "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/naming-and-layout.html#header-rules"),
             ("clang-format", "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/formatting.html#clang-format")],
        )

    def test_sarif_needs_check(self):
        with self.assertRaises(SystemExit):
            with contextlib.redirect_stderr(io.StringIO()):
                formatter.main(["--sarif", os.path.join(self.tmp, "x.sarif"), self.good])

    def test_missing_path_is_an_error(self):
        code, _ = _run(["--check", os.path.join(self.tmp, "nope.hpp")], self.tmp)
        self.assertEqual(code, 2)

    def test_argument_parsing(self):
        code, _ = _run(["--check", self.good], self.tmp)
        self.assertEqual(code, 0)
        with self.assertRaises(SystemExit):
            with contextlib.redirect_stderr(io.StringIO()):
                formatter.main(["--bogus"])

needs_clang_format = unittest.skipIf(formatter.CLANG_FORMAT_EXE is None, "clang-format not found")

UGLY = "int f(){return 1;}\n"
UGLY_FORMATTED = "int\nf()\n{\n    return 1;\n}\n"


class IncrementalReformatTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = gitrepo.init(os.path.join(self.tmp, "repo"))
        shutil.copy(os.path.join(REPO_ROOT, ".clang-format"), self.repo)
        self.lib = os.path.join(self.repo, "src", "mrdocs")
        self.root = str(self.repo)

    def _write(self, relative, text):
        return str(gitrepo.write(self.repo, relative, text))

    def _read(self, relative):
        with open(os.path.join(self.repo, relative), newline="") as f:
            return f.read()

    def _change(self):
        return changed_lines.Change(self.repo, changed_lines.resolve_base(self.repo)[0])

    def _run(self, argv, **kwargs):
        return _run(argv, self.root, change=self._change(), **kwargs)

    @needs_clang_format
    def test_only_the_changed_lines_are_formatted(self):
        self._write("src/mrdocs/A.cpp", UGLY)
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", UGLY + "\n" + "int g(){return 2;}\n")
        code, out = self._run([])
        self.assertEqual(code, 0)
        self.assertEqual(self._read("src/mrdocs/A.cpp"),
                         UGLY + "\n" + "int\ng()\n{\n    return 2;\n}\n")
        self.assertIn("src/mrdocs/A.cpp: updated (clang-format)", out)

    @needs_clang_format
    def test_a_clang_format_error_fails_the_check(self):
        self._write("src/mrdocs/A.cpp", "int a;\n")
        gitrepo.commit_base(self.repo)
        self._write(".clang-format", "ReflowComments: NotAnOption\n")
        self._write("src/mrdocs/A.cpp", "int a;\nint b;\n")
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/A.cpp: (clang-format failed", out)
        self.assertIn("1 skipped", out)

    @needs_clang_format
    def test_check_reports_changed_lines_only_and_writes_nothing(self):
        self._write("src/mrdocs/A.cpp", UGLY)
        gitrepo.commit_base(self.repo)
        text = UGLY + "\n" + "int g(){return 2;}\n"
        self._write("src/mrdocs/A.cpp", text)
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/A.cpp:3: clang-format would change this line (clang-format)", out)
        self.assertNotIn("A.cpp:1:", out)
        self.assertEqual(self._read("src/mrdocs/A.cpp"), text)

    @needs_clang_format
    def test_a_file_the_branch_did_not_change_is_left_alone(self):
        self._write("src/mrdocs/Old.cpp", UGLY)
        self._write("src/mrdocs/Edited.cpp", "int a;\n")
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/Edited.cpp", "int a;\n" + UGLY)
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("Edited.cpp:2:", out)
        self.assertNotIn("Old.cpp", out)
        self.assertIn("1 files total, 1 would change", out)

    @needs_clang_format
    def test_all_formats_every_line_of_every_file(self):
        self._write("src/mrdocs/Old.cpp", UGLY)
        gitrepo.commit_base(self.repo)
        code, out = _run(["--check"], self.root)
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/Old.cpp:1:", out)
        _run([], self.root)
        self.assertEqual(self._read("src/mrdocs/Old.cpp"), UGLY_FORMATTED)

    @needs_clang_format
    def test_named_paths_are_limited_to_their_changed_lines_unless_all(self):
        path = self._write("src/mrdocs/A.cpp", UGLY)
        other = self._write("src/mrdocs/B.cpp", UGLY)
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", UGLY + "int g(){return 2;}\n")
        code, out = self._run(["--check", path, other])
        self.assertEqual(code, 1)
        self.assertIn("A.cpp:2:", out)
        self.assertNotIn("A.cpp:1:", out)
        self.assertNotIn("B.cpp", out)
        code, out = _run(["--check", other], self.root)
        self.assertIn("B.cpp:1:", out)

    @needs_clang_format
    def test_golden_fixtures_are_never_formatted(self):
        consumer = "tests/integration/cmake/executable/include/example/calc.hpp"
        self._write(consumer, "#ifndef EXAMPLE_CALC_HPP\n#define EXAMPLE_CALC_HPP\n#endif\n")
        fixture = "tests/golden/fixtures/symbols/a.cpp"
        sibling = "tests/golden/Support/A.cpp"
        self._write(fixture, UGLY)
        self._write(sibling, UGLY)
        gitrepo.commit_base(self.repo)
        self._write(fixture, UGLY + "int g(){return 2;}\n")
        self._write(sibling, UGLY + "int g(){return 2;}\n")
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("tests/golden/Support/A.cpp:", out)
        self.assertNotIn("fixtures", out)
        self.assertNotIn("integration/cmake", out)
        path = os.path.join(self.repo, fixture)
        code, out = self._run(["--check", path])
        self.assertEqual(code, 0)
        self.assertIn("[skip]", out)
        code, out = _run(["--check"], self.root)
        self.assertNotIn("fixtures", out)
        code, out = _run(["--check", os.path.join(self.repo, "tests")], self.root)
        self.assertIn("tests/golden/Support/A.cpp:", out)
        self.assertNotIn("fixtures", out)
        _run([], self.root)
        self.assertEqual(self._read(fixture), UGLY + "int g(){return 2;}\n")
        self.assertEqual(self._read(sibling), UGLY_FORMATTED + "int\ng()\n{\n    return 2;\n}\n")

    @needs_clang_format
    def test_an_untracked_file_is_all_new(self):
        self._write("src/mrdocs/Old.cpp", "int a;\n")
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/Fresh.cpp", UGLY)
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/Fresh.cpp:1:", out)

    @needs_clang_format
    def test_clang_format_sorts_the_include_block_a_changed_line_touches(self):
        block = "#include <algorithm>\n#include <vector>\n"
        self._write("src/mrdocs/A.cpp", block + "\nint x;\n")
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", block + "#include <map>\n\nint x;\n")
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/A.cpp:3: clang-format would change this line (clang-format)", out)
        self._run([])
        self.assertEqual(self._read("src/mrdocs/A.cpp"),
                         "#include <algorithm>\n#include <map>\n#include <vector>\n\nint x;\n")

    @needs_clang_format
    def test_an_unsorted_include_block_no_changed_line_touches_stays(self):
        text = "#include <vector>\n#include <algorithm>\n\nint x;\n"
        self._write("src/mrdocs/A.cpp", text)
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", text + "int y;\n")
        code, out = self._run(["--check"])
        self.assertEqual(code, 0, out)
        self._run([])
        self.assertEqual(self._read("src/mrdocs/A.cpp"), text + "int y;\n")

    @needs_clang_format
    def test_an_old_include_that_moves_past_a_new_one_in_its_place_passes_the_check(self):
        self._write("src/mrdocs/A.cpp", "#include <vector>\n#include <algorithm>\n\nint x;\n")
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", "#include <vector>\n#include <algorithm>\n#include <map>\n\nint x;\n")
        code, out = self._run(["--check"])
        self.assertEqual(code, 1, out)
        self.assertIn("src/mrdocs/A.cpp:3: clang-format would change this line (clang-format)", out)
        self.assertNotIn("A.cpp:1:", out)

    def _check_then_write(self, before, after):
        self._write("src/mrdocs/A.cpp", before)
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", after)
        code, out = self._run(["--check"])
        self._run([])
        return code, out, self._read("src/mrdocs/A.cpp")

    @needs_clang_format
    def test_a_failing_check_matches_a_write_that_changes_the_file(self):
        after = "#include <vector>\n#include <algorithm>\n#include <map>\n\nint x;\n"
        code, out, written = self._check_then_write("#include <vector>\n#include <algorithm>\n\nint x;\n", after)
        self.assertEqual(code, 1, out)
        self.assertNotEqual(written, after)

    @needs_clang_format
    def test_a_clean_check_means_the_write_changes_nothing(self):
        after = "#include <vector>\n#include <algorithm>\n\nint x;\nint y;\n"
        code, out, written = self._check_then_write("#include <vector>\n#include <algorithm>\n\nint x;\n", after)
        self.assertEqual(code, 0, out)
        self.assertEqual(written, after)

    @needs_clang_format
    def test_line_numbers_are_the_ones_git_counts(self):
        base = "int a;\nint b; // x\x0cy\nint c; // \u2028\n"
        self._write("src/mrdocs/A.cpp", base)
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", base + UGLY)
        code, out = self._run(["--check"])
        self.assertEqual(code, 1, out)
        self.assertIn("src/mrdocs/A.cpp:4: clang-format would change this line (clang-format)", out)
        self._run([])
        self.assertEqual(self._read("src/mrdocs/A.cpp"), base + UGLY_FORMATTED)

    def _renamed_header(self, commit):
        guard = ("#ifndef MRDOCS_LIB_OLD_HPP\n#define MRDOCS_LIB_OLD_HPP\n\nint old_line;\nint more;\n\n"
                 "#endif // MRDOCS_LIB_OLD_HPP\n")
        self._write("src/mrdocs/Old.hpp", guard)
        gitrepo.commit_base(self.repo)
        os.makedirs(os.path.join(self.repo, "src", "mrdocs", "sub"))
        os.rename(os.path.join(self.repo, "src", "mrdocs", "Old.hpp"),
                  os.path.join(self.repo, "src", "mrdocs", "sub", "New.hpp"))
        if commit:
            gitrepo.git(self.repo, "add", ".")
            gitrepo.git(self.repo, "commit", "-q", "-m", "move")
        code, out = self._run(["--check"])
        self.assertEqual(code, 1, out)
        self.assertIn("src/mrdocs/sub/New.hpp:1: include guard MRDOCS_LIB_OLD_HPP would become "
                      "MRDOCS_LIB_SUB_NEW_HPP (header-rules)", out)
        self._run([])
        self.assertIn("MRDOCS_LIB_SUB_NEW_HPP", self._read("src/mrdocs/sub/New.hpp"))
        code, out = self._run(["--check"])
        self.assertEqual(code, 0, out)

    def test_a_committed_renamed_header_gets_the_guard_of_its_new_path(self):
        self._renamed_header(True)

    def test_an_uncommitted_renamed_header_gets_the_guard_of_its_new_path(self):
        self._renamed_header(False)

    def _public_repo(self):
        self._write("include/mrdocs/Support/Path.hpp", "int p;\n")
        self._write("src/mrdocs/Support/Old.cpp", '#include "../../../include/mrdocs/Support/Path.hpp"\nint a;\n')
        gitrepo.commit_base(self.repo)

    def test_the_include_spelling_is_rewritten_on_changed_lines_only(self):
        self._public_repo()
        self._write("src/mrdocs/Support/Old.cpp",
                    '#include "../../../include/mrdocs/Support/Path.hpp"\n'
                    '#include "../../../include/mrdocs/Support/Path.hpp"\nint a;\n')
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/Support/Old.cpp:2: include would be spelled <mrdocs/Support/Path.hpp> (header-rules)", out)
        self.assertNotIn("Old.cpp:1:", out)
        self._run([])
        lines = self._read("src/mrdocs/Support/Old.cpp").splitlines()
        self.assertEqual(lines[0], '#include "../../../include/mrdocs/Support/Path.hpp"')
        self.assertEqual(lines[1], "#include <mrdocs/Support/Path.hpp>")

    def test_the_header_guard_is_fixed_only_when_a_guard_line_changed(self):
        guard = "#ifndef WRONG\n#define WRONG\n\nint old_line;\n\n#endif\n"
        self._write("src/mrdocs/A.hpp", guard)
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.hpp", guard.replace("old_line", "new_line"))
        code, out = self._run(["--check"])
        self.assertEqual(code, 0, out)
        self._write("src/mrdocs/A.hpp", guard.replace("#define WRONG", "#define  WRONG"))
        code, out = self._run(["--check"])
        self.assertEqual(code, 1)
        self.assertIn("src/mrdocs/A.hpp:1: include guard WRONG would become MRDOCS_LIB_A_HPP (header-rules)", out)

    def test_sarif_has_the_two_rules_and_the_line_of_each_finding(self):
        self._write("src/mrdocs/A.hpp", "int a;\n")
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.hpp", "#ifndef WRONG\n#define WRONG\nint a;\n#endif\n")
        target = os.path.join(self.tmp, "out.sarif")
        code, _ = self._run(["--check"], sarif_path=target)
        self.assertEqual(code, 1)
        with open(target, encoding="utf-8") as f:
            run = json.load(f)["runs"][0]
        self.assertEqual([r["id"] for r in run["tool"]["driver"]["rules"]], ["header-rules", "clang-format"])
        self.assertEqual({r["ruleId"] for r in run["results"]}, {"header-rules"} | (
            {"clang-format"} if formatter.CLANG_FORMAT_EXE and any(
                r["ruleId"] == "clang-format" for r in run["results"]) else set()))
        self.assertIn(1, [r["locations"][0]["physicalLocation"]["region"]["startLine"] for r in run["results"]])

    @needs_clang_format
    def test_sarif_gives_a_clang_format_line_the_clang_format_rule(self):
        self._write("src/mrdocs/A.cpp", "int a;\n")
        gitrepo.commit_base(self.repo)
        self._write("src/mrdocs/A.cpp", "int a;\n" + UGLY)
        target = os.path.join(self.tmp, "out.sarif")
        self._run(["--check"], sarif_path=target)
        with open(target, encoding="utf-8") as f:
            (result,) = json.load(f)["runs"][0]["results"]
        self.assertEqual(result["ruleId"], "clang-format")
        self.assertEqual(result["locations"][0]["physicalLocation"]["region"]["startLine"], 2)

    def test_main_needs_a_base_unless_all(self):
        original = formatter.REPO_ROOT
        self.addCleanup(setattr, formatter, "REPO_ROOT", original)
        self._write("src/mrdocs/A.cpp", "int a;\n")
        gitrepo.git(self.repo, "add", ".")
        gitrepo.git(self.repo, "commit", "-q", "-m", "one")
        formatter.REPO_ROOT = self.root
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(formatter.main(["--check"]), 1)
            self.assertIn("format: no base found, tried upstream/develop, origin/develop; use --base REF or --all",
                          out.getvalue())
            self.assertEqual(formatter.main(["--check", "--base", "HEAD"]), 0)
            self.assertEqual(formatter.main(["--check", "--all"]), 0)


if __name__ == "__main__":
    unittest.main()
