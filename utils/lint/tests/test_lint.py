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

"""Tests for utils/lint/lint.py, run on stub guards."""

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
import lint
from tests import gitrepo

STUB = '''
import sys
sys.path.insert(0, %(lint)r)
import sarif
args = sys.argv[1:]
print("stub %(name)s", args)
if %(writes)r and "--sarif" in args:
    sarif.Log("%(name)s", ["%(name)s"]).write(args[args.index("--sarif") + 1])
sys.exit(%(code)d)
'''

NAMES = ("layout", "readme-length", "page-length", "section-title", "header-rules",
         "unit-test-mirror", "yaml-schema", "bootstrap-options", "commands-registry", "json-schemas")
INCREMENTAL = NAMES
TAKE_THE_SCOPE = ("layout", "readme-length", "page-length", "section-title", "header-rules",
                  "unit-test-mirror")
BY_PATHS = ("yaml-schema", "bootstrap-options", "commands-registry", "json-schemas")


class LintTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def _guards(self, failing=(), needs=None):
        guards = []
        for real in lint.GUARDS:
            script = self.tmp / (real.name + ".py")
            script.write_text(STUB % {
                "lint": os.path.dirname(HERE), "name": real.name,
                "writes": real.writes_sarif, "code": 1 if real.name in failing else 0})
            guards.append(lint.Guard(
                real.name, [[script]], incremental=real.incremental, takes_files=real.takes_files,
                writes_sarif=real.writes_sarif, needs=needs if real.name == "json-schemas" else None,
                hint=real.hint, inputs=real.inputs, outputs=real.outputs))
        return guards

    def _run(self, guards, *args, **kwargs):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            failed = lint.run_guards(guards, *args, **kwargs)
        return failed, out.getvalue()

    def _stub_args(self, output):
        return {line.split()[1]: line for line in output.splitlines() if line.startswith("stub ")}

    def test_every_guard_passes(self):
        failed, _ = self._run(self._guards())
        self.assertEqual(failed, [])

    def test_a_failing_guard_does_not_stop_the_others(self):
        failed, output = self._run(self._guards(failing=("readme-length", "header-rules", "yaml-schema")))
        self.assertEqual(failed, ["readme-length", "header-rules", "yaml-schema"])
        self.assertIn("stub json-schemas", output)

    def test_main_exit_status(self):
        original = lint.GUARDS
        self.addCleanup(setattr, lint, "GUARDS", original)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            lint.GUARDS = self._guards()
            self.assertEqual(lint.main(["--all"]), 0)
            lint.GUARDS = self._guards(failing=("layout",))
            self.assertEqual(lint.main(["--all"]), 1)
        self.assertIn("lint: failed: layout", out.getvalue())

    def test_sarif_holds_one_run_per_guard(self):
        target = self.tmp / "out" / "lint.sarif"
        failed, _ = self._run(self._guards(failing=("page-length", "commands-registry")), str(target))
        self.assertEqual(failed, ["page-length", "commands-registry"])
        doc = json.loads(target.read_text(encoding="utf-8"))
        self.assertEqual([run["tool"]["driver"]["name"] for run in doc["runs"]], list(NAMES))

    def test_a_guard_without_sarif_of_its_own_gets_a_run_from_lint(self):
        target = self.tmp / "lint.sarif"
        guards = self._guards(failing=("yaml-schema",))
        self._run(guards, str(target))
        runs = {run["tool"]["driver"]["name"]: run
                for run in json.loads(target.read_text(encoding="utf-8"))["runs"]}
        self.assertEqual(runs["bootstrap-options"]["results"], [])
        (result,) = runs["yaml-schema"]["results"]
        self.assertEqual(result["ruleId"], "yaml-schema")
        self.assertEqual(result["level"], "error")
        self.assertIn("stub yaml-schema", result["message"]["text"])
        self.assertEqual(runs["yaml-schema"]["tool"]["driver"]["rules"][0]["helpUri"],
                         "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants/codegen.html#yaml-schema")

    def test_the_scope_reaches_only_the_incremental_guards(self):
        _, output = self._run(self._guards(), None, None, ["--base", "abc123"])
        args = self._stub_args(output)
        for name in NAMES:
            if name in TAKE_THE_SCOPE:
                self.assertIn("'--base', 'abc123'", args[name], name)
            else:
                self.assertNotIn("abc123", args[name], name)
        _, output = self._run(self._guards(), None, None, ["--all"])
        args = self._stub_args(output)
        self.assertEqual([name for name in NAMES if "'--all'" in args[name]], list(TAKE_THE_SCOPE))

    def test_every_guard_is_incremental(self):
        self.assertEqual([g.name for g in lint.GUARDS if g.incremental or g.inputs or g.outputs],
                         list(INCREMENTAL))
        self.assertEqual([g.name for g in lint.GUARDS if g.incremental], list(TAKE_THE_SCOPE))
        self.assertEqual([g.name for g in lint.GUARDS if g.inputs or g.outputs], list(BY_PATHS))

    def test_the_declared_paths_match_files_of_the_repository(self):
        listed = gitrepo.subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"], cwd=lint.REPO, text=True,
            stdout=gitrepo.subprocess.PIPE).stdout.split("\n")
        for guard in lint.GUARDS:
            for pattern in guard.inputs + guard.outputs:
                self.assertTrue(any(lint.fnmatch.fnmatchcase(path, pattern) for path in listed),
                                "%s: %s matches no file" % (guard.name, pattern))

    def test_files_reach_only_the_header_rules_guard(self):
        _, output = self._run(self._guards(), None, ["a/b.hpp", "c"])
        args = self._stub_args(output)
        self.assertIn("'a/b.hpp', 'c'", args["header-rules"])
        for name in NAMES:
            if name != "header-rules":
                self.assertNotIn("a/b.hpp", args[name])

    def test_a_missing_program_skips_the_guard_and_says_so(self):
        target = self.tmp / "lint.sarif"
        skipped = []
        failed, output = self._run(self._guards(needs="no-such-program-xyz"), str(target), skipped=skipped)
        self.assertEqual(failed, [])
        self.assertEqual(skipped, ["json-schemas"])
        self.assertIn("warning: json-schemas did not run: no-such-program-xyz not found on PATH "
                      "(install Node.js to run it)", output)
        self.assertNotIn("stub json-schemas", output)
        runs = json.loads(target.read_text(encoding="utf-8"))["runs"]
        self.assertEqual(len(runs), len(NAMES))
        (result,) = runs[-1]["results"]
        self.assertEqual(result["level"], "warning")
        self.assertIn("did not run: no-such-program-xyz not found on PATH", result["message"]["text"])

    def test_main_reports_the_skipped_guards(self):
        original = lint.GUARDS
        self.addCleanup(setattr, lint, "GUARDS", original)
        lint.GUARDS = self._guards(needs="no-such-program-xyz")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(lint.main(["--all"]), 0)
        self.assertEqual(out.getvalue().splitlines()[-1], "lint: ok, but 1 guard did not run: json-schemas")
        self.assertEqual(out.getvalue().count("did not run"), 2)

    def test_main_lists_the_failures_and_the_guards_that_did_not_run(self):
        original = lint.GUARDS
        self.addCleanup(setattr, lint, "GUARDS", original)
        lint.GUARDS = self._guards(failing=("layout",), needs="no-such-program-xyz")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(lint.main(["--all"]), 1)
        self.assertEqual(out.getvalue().splitlines()[-1],
                         "lint: failed: layout; 1 guard did not run: json-schemas")

    def test_main_counts_two_guards_that_did_not_run(self):
        guards = [lint.Guard(name, [[self.tmp / "x.py"]], needs="no-such-program-xyz")
                  for name in ("one", "two")]
        out = io.StringIO()
        original = lint.GUARDS
        self.addCleanup(setattr, lint, "GUARDS", original)
        lint.GUARDS = guards
        with contextlib.redirect_stdout(out):
            self.assertEqual(lint.main(["--all"]), 0)
        self.assertEqual(out.getvalue().splitlines()[-1], "lint: ok, but 2 guards did not run: one, two")

    def _branch(self, *touched):
        """Return a `Change` of a repository whose branch wrote the files `touched`."""
        repo = gitrepo.init(self.tmp / "branch")
        gitrepo.write(repo, "README", "x\n")
        gitrepo.commit_base(repo)
        for path in touched:
            gitrepo.write(repo, path, "edited\n")
        return changed_lines.Change(repo, changed_lines.resolve_base(repo)[0])

    def _by_paths(self, name, touched, failing=False):
        change = self._branch(*touched)
        guards = [g for g in self._guards(failing=(name,) if failing else ()) if g.name == name]
        failed, output = self._run(guards, change=change)
        return failed, output

    def test_a_guard_the_branch_did_not_touch_is_not_affected_and_passes(self):
        for guard in lint.GUARDS:
            if guard.name not in BY_PATHS:
                continue
            with self.subTest(guard.name):
                shutil.rmtree(self.tmp / "branch", ignore_errors=True)
                failed, output = self._by_paths(guard.name, ["docs/other.adoc"], failing=True)
                self.assertEqual(failed, [])
                self.assertIn("%s: not affected" % guard.name, output)
                self.assertNotIn("stub", output)

    def test_a_guard_whose_input_or_output_changed_runs_and_passes(self):
        for guard in lint.GUARDS:
            if guard.name not in BY_PATHS:
                continue
            for touched in (guard.inputs[0].replace("*", "a"), guard.outputs[0] if guard.outputs
                            else guard.inputs[-1]):
                with self.subTest(guard.name, touched=touched):
                    shutil.rmtree(self.tmp / "branch", ignore_errors=True)
                    failed, output = self._by_paths(guard.name, [touched])
                    self.assertEqual(failed, [])
                    self.assertIn("stub %s" % guard.name, output)
                    self.assertNotIn("not affected", output)

    def test_a_guard_whose_input_or_output_changed_runs_and_fails(self):
        for guard in lint.GUARDS:
            if guard.name not in BY_PATHS:
                continue
            with self.subTest(guard.name):
                shutil.rmtree(self.tmp / "branch", ignore_errors=True)
                touched = guard.outputs[0] if guard.outputs else guard.inputs[0]
                failed, output = self._by_paths(guard.name, [touched], failing=True)
                self.assertEqual(failed, [guard.name])

    def test_a_deleted_input_counts_as_changed(self):
        repo = gitrepo.init(self.tmp / "branch")
        gitrepo.write(repo, "src/mrdocs/ConfigOptions.json", "[]\n")
        gitrepo.commit_base(repo)
        os.remove(repo / "src/mrdocs/ConfigOptions.json")
        change = changed_lines.Change(repo, changed_lines.resolve_base(repo)[0])
        (guard,) = [g for g in self._guards() if g.name == "yaml-schema"]
        failed, output = self._run([guard], change=change)
        self.assertIn("stub yaml-schema", output)

    def test_a_guard_that_is_not_affected_does_not_need_its_program(self):
        change = self._branch("docs/other.adoc")
        skipped = []
        failed, output = self._run(self._guards(needs="no-such-program-xyz"), change=change, skipped=skipped)
        self.assertEqual(skipped, [])
        self.assertIn("json-schemas: not affected", output)
        self.assertNotIn("warning", output)

    def test_all_runs_the_guards_that_are_not_affected(self):
        _, output = self._run(self._guards(), None, None, ["--all"])
        for name in BY_PATHS:
            self.assertIn("stub %s" % name, output)

    def test_a_guard_that_crashes_still_has_a_run_in_the_sarif(self):
        guards = self._guards()
        crash = self.tmp / "layout.py"
        crash.write_text("import sys\nsys.exit(3)\n")
        guards[0] = lint.Guard("layout", [[crash]], writes_sarif=True)
        target = self.tmp / "crash.sarif"
        failed, _ = self._run(guards, str(target))
        self.assertEqual(failed, ["layout"])
        doc = json.loads(target.read_text(encoding="utf-8"))
        self.assertEqual(len(doc["runs"]), len(NAMES))
        (result,) = doc["runs"][0]["results"]
        self.assertEqual(result["level"], "error")
        self.assertIn("status 3", result["message"]["text"])

    def test_a_guard_with_several_commands_fails_when_one_fails(self):
        ok = self.tmp / "ok.py"
        ok.write_text("print('first')\n")
        bad = self.tmp / "bad.py"
        bad.write_text("import sys\nprint('second')\nsys.exit(1)\n")
        failed, output = self._run([lint.Guard("two", [[ok], [bad], [ok]])])
        self.assertEqual(failed, ["two"])
        self.assertEqual(output.count("first"), 2)

    def test_no_base_without_all_fails(self):
        original = lint.REPO
        self.addCleanup(setattr, lint, "REPO", original)
        repo = gitrepo.init(self.tmp / "repo")
        gitrepo.write(repo, "a.txt", "x\n")
        gitrepo.git(repo, "add", ".")
        gitrepo.git(repo, "commit", "-q", "-m", "one")
        lint.REPO = repo
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(lint.main([]), 1)
        self.assertIn("lint: no base found, tried upstream/develop, origin/develop; use --base REF or --all",
                      out.getvalue())

    def test_main_passes_the_resolved_base_to_the_incremental_guards(self):
        original, original_guards = lint.REPO, lint.GUARDS
        self.addCleanup(setattr, lint, "REPO", original)
        self.addCleanup(setattr, lint, "GUARDS", original_guards)
        repo = gitrepo.init(self.tmp / "repo")
        gitrepo.write(repo, "a.txt", "x\n")
        gitrepo.commit_base(repo)
        commit = gitrepo.subprocess.run(["git", "rev-parse", "HEAD"], cwd=repo, text=True,
                                        stdout=gitrepo.subprocess.PIPE).stdout.strip()
        lint.REPO = repo
        lint.GUARDS = self._guards()
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(lint.main([]), 0)
        self.assertIn("'--base', %r" % commit, self._stub_args(out.getvalue())["header-rules"])

    def test_the_real_guards_run_in_the_documented_order(self):
        self.assertEqual([guard.name for guard in lint.GUARDS], list(NAMES))
        commands = {guard.name: [[Path(str(c[0])).name] + [str(a) for a in c[1:]]
                                 for c in guard.commands] for guard in lint.GUARDS}
        self.assertEqual(commands["layout"], [["check_layout.py"]])
        self.assertEqual(commands["header-rules"], [["check_header_rules.py"]])
        self.assertEqual(commands["yaml-schema"], [["generate-yaml-schema.py", "--check"]])
        self.assertEqual(commands["bootstrap-options"], [["generate_bootstrap_options.py", "--check"]])
        self.assertEqual(commands["commands-registry"], [["check_commands_registry.py"]])
        self.assertEqual(
            commands["json-schemas"],
            [["npx", "-y", "-p", "ajv-cli", "--", "ajv", "compile", "-s",
              "docs/modules/ROOT/attachments/schemas/config/mrdocs.schema.json"],
             ["npx", "-y", "-p", "ajv-cli", "--", "ajv", "compile", "-s",
              "docs/modules/ROOT/attachments/schemas/generators/mrdocs.schema.json"]])
        self.assertEqual([g.name for g in lint.GUARDS if g.needs], ["json-schemas"])
        for guard in lint.GUARDS:
            for command in guard.commands:
                if isinstance(command[0], Path):
                    self.assertTrue(command[0].is_file(), command[0])
        for schema in ("config", "generators"):
            self.assertTrue((lint.REPO / lint.SCHEMAS / schema / "mrdocs.schema.json").is_file())


if __name__ == "__main__":
    unittest.main()
