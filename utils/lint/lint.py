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

"""Run every guard that needs no build, and report all of them in one run.

The guards run in this order, each as its own process. A failing guard does
not stop the run, so one run reports everything, and the exit status is 1 when
any guard failed and 0 otherwise.

1. `layout`: `check_layout.py`.
2. `readme-length`: `check_readme_length.py`.
3. `page-length`: `check_page_length.py`.
4. `section-title`: `check_section_titles.py`.
5. `header-rules`: `check_header_rules.py`, which checks that a header guard
   matches its path and a public header is included as `<mrdocs/...>`. The
   rules live in `header_rules.py`, which `utils/format/format.py` imports to
   fix the same lines. Formatting is advice, so no guard fails on it.
6. `unit-test-mirror`: `check_unit_test_mirror.py`, which checks that each
   unit test file `tests/unit/<path>.cpp` has a header `<path>.hpp` under
   `include/mrdocs/` or `src/mrdocs/`.
7. `yaml-schema`: `utils/codegen/generate-yaml-schema.py --check`.
8. `bootstrap-options`: `utils/docs/generate_bootstrap_options.py --check`.
9. `commands-registry`: `utils/docs/check_commands_registry.py`.
10. `json-schemas`: `npx -y -p ajv-cli -- ajv compile -s SCHEMA` on the config
    schema and the generator schema under docs/modules/ROOT/attachments/schemas.
    Without `npx` on the path the guard did not run: the run prints a warning
    that names the guard, the missing program and how to get it, and its last
    line says how many guards did not run. A guard that did not run is neither
    a pass nor a failure, so the exit status stays 0.

Every guard judges only what the change touched. The change is everything
since the merge base of HEAD with the default branch of `upstream` (else
`upstream/develop`, else `origin/develop`), counting uncommitted edits and
untracked files that git does not ignore. Guards 1 to 6 judge the entries,
READMEs, pages, section titles, source lines and unit test files the change
added, renamed, removed or edited. Each of guards 7 to 10 declares the paths it
reads and writes, plus `lint.py` itself, as globs where `*` also matches `/`,
and runs only when the change touched one of them. Otherwise it prints `<guard>: not affected` and
passes. When it runs it compares whole files, and takes seconds. `--base REF`
picks another base and `--all` makes every guard judge the whole tree.
`--files PATH...` limits the header rules guard to those files and
directories. When no base resolves and `--all` is not given, the run names the
refs it tried and exits 1.

With `--sarif FILE` the run also writes one SARIF file with one run per guard.

The portability check of the Linux release needs a built package and runs
apart, from `.github/scripts/linux-portability/check-linux-portability.sh`.

    python3 -I utils/lint/lint.py
    python3 -I utils/lint/lint.py --sarif lint.sarif
    python3 -I utils/lint/lint.py --all
    python3 -I utils/lint/lint.py --base upstream/develop
    python3 -I utils/lint/lint.py --files include/mrdocs/Platform.hpp
"""

import argparse
import fnmatch
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, NamedTuple, Optional, Tuple

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import changed_lines
import sarif

REPO = HERE.parent.parent
SCHEMAS = "docs/modules/ROOT/attachments/schemas"
LINT = "utils/lint/lint.py"


class Guard(NamedTuple):
    """One guard of the run.

    `commands` lists the commands to run; a `Path` first element is a Python
    script, run with `python3 -I`. `incremental` guards take `--base REF` or
    `--all` and judge only the change themselves, `takes_files` guards take the
    `--files` paths, and `writes_sarif` guards take `--sarif FILE`; for the
    others `lint.py` writes the run. A guard with `inputs` or `outputs` (globs
    of repo-relative paths, where `*` also matches `/`) compares whole files,
    so `lint.py` runs it only when the change touched one of them. A guard
    whose `needs` program is not on the path did not run, and `hint` says how
    to get the program.
    """
    name: str
    commands: List[list]
    incremental: bool = False
    takes_files: bool = False
    writes_sarif: bool = False
    needs: Optional[str] = None
    hint: str = ""
    inputs: Tuple[str, ...] = ()
    outputs: Tuple[str, ...] = ()

    def affected_by(self, paths):
        """Return whether one of `paths` is an input or an output of the guard."""
        patterns = self.inputs + self.outputs
        return any(fnmatch.fnmatchcase(path, pattern) for path in paths for pattern in patterns)


GUARDS = [
    Guard("layout", [[HERE / "check_layout.py"]], incremental=True, writes_sarif=True),
    Guard("readme-length", [[HERE / "check_readme_length.py"]],
          incremental=True, writes_sarif=True),
    Guard("page-length", [[HERE / "check_page_length.py"]],
          incremental=True, writes_sarif=True),
    Guard("section-title", [[HERE / "check_section_titles.py"]],
          incremental=True, writes_sarif=True),
    Guard("header-rules", [[HERE / "check_header_rules.py"]],
          incremental=True, takes_files=True, writes_sarif=True),
    Guard("unit-test-mirror", [[HERE / "check_unit_test_mirror.py"]],
          incremental=True, writes_sarif=True),
    Guard("yaml-schema", [[REPO / "utils/codegen/generate-yaml-schema.py", "--check"]],
          inputs=("src/mrdocs/ConfigOptions.json", "utils/codegen/generate-yaml-schema.py", LINT),
          outputs=(SCHEMAS + "/config/mrdocs.schema.json",)),
    Guard("bootstrap-options", [[REPO / "utils/docs/generate_bootstrap_options.py", "--check"]],
          inputs=("utils/bootstrap/src/*", "utils/docs/generate_bootstrap_options.py", LINT),
          outputs=("docs/modules/ROOT/partials/bootstrap-options.adoc",)),
    Guard("commands-registry", [[REPO / "utils/docs/check_commands_registry.py"]],
          inputs=("include/mrdocs/Metadata/DocComment/*.inc",
                  "src/mrdocs/AST/ExtractDocComment.cpp",
                  "src/mrdocs/Metadata/Finalizers/DocComment/parseInlines.hpp",
                  "tests/golden/fixtures/snippets/commands/*",
                  "utils/docs/check_commands_registry.py", LINT),
          outputs=("docs/modules/ROOT/partials/commands-registry.json",)),
    Guard("json-schemas",
          [["npx", "-y", "-p", "ajv-cli", "--", "ajv", "compile", "-s",
            SCHEMAS + "/config/mrdocs.schema.json"],
           ["npx", "-y", "-p", "ajv-cli", "--", "ajv", "compile", "-s",
            SCHEMAS + "/generators/mrdocs.schema.json"]],
          needs="npx", hint="install Node.js to run it",
          inputs=(SCHEMAS + "/config/mrdocs.schema.json",
                  SCHEMAS + "/generators/mrdocs.schema.json", LINT)),
]


def _argv(command, extra):
    """Return the argument vector of `command`, followed by `extra`."""
    if isinstance(command[0], Path):
        return [sys.executable, "-I", str(command[0])] + [str(a) for a in command[1:]] + list(extra)
    return [str(a) for a in command] + list(extra)


def _summary(output, limit=3):
    """Return the first non-empty lines of `output`, joined, for a SARIF message."""
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    return " | ".join(lines[:limit])


def run_guards(guards, sarif_path=None, files=None, scope=("--all",), skipped=None, cwd=REPO, change=None):
    """Run `guards` in order and return the names of those that failed.

    `scope` is the arguments for the incremental guards: `--all`, or
    `--base REF`. `files` is the list of paths for the guards that take
    files, or None. A guard with inputs or outputs runs only when `change`, a
    `changed_lines.Change`, touched one of them; None means every guard runs. A
    guard whose needed program is missing did not run: a warning names it and
    its name is appended to `skipped`. When `sarif_path` is set, the runs of
    the guards are merged into it.
    """
    failed = []
    scratch = Path(tempfile.mkdtemp(prefix="mrdocs-lint-"))
    try:
        parts = []
        for guard in guards:
            part = scratch / (guard.name + ".sarif")
            parts.append(part)
            print("== %s" % guard.name, flush=True)
            if (guard.inputs or guard.outputs) and change is not None \
                    and not guard.affected_by(change.paths()):
                print("%s: not affected" % guard.name)
                sarif.Log(guard.name, [guard.name]).write(part)
                continue
            if guard.needs and shutil.which(guard.needs) is None:
                reason = "%s not found on PATH" % guard.needs
                if guard.hint:
                    reason += " (%s)" % guard.hint
                print("warning: %s did not run: %s" % (guard.name, reason))
                if skipped is not None:
                    skipped.append(guard.name)
                log = sarif.Log(guard.name, [guard.name])
                log.add(guard.name, "did not run: %s" % reason, level="warning")
                log.write(part)
                continue
            extra = list(scope) if guard.incremental else []
            if guard.writes_sarif:
                extra += ["--sarif", str(part)]
            if guard.takes_files and files:
                extra += list(files)
            output = ""
            code = 0
            for command in guard.commands:
                try:
                    done = subprocess.run(_argv(command, extra), cwd=str(cwd), stdout=subprocess.PIPE,
                                          stderr=subprocess.STDOUT, text=True)
                except OSError as error:
                    output += "%s\n" % error
                    code = 127
                    break
                output += done.stdout
                if done.returncode != 0:
                    code = done.returncode
            print(output, end="" if output.endswith("\n") or not output else "\n", flush=True)
            if code != 0:
                failed.append(guard.name)
            if guard.writes_sarif:
                if code != 0 and not part.exists():
                    crash = sarif.Log(guard.name, [guard.name])
                    crash.add(guard.name, "%s exited with status %d and wrote no findings"
                              % (Path(str(guard.commands[0][0])).name, code))
                    crash.write(part)
            else:
                log = sarif.Log(guard.name, [guard.name])
                if code != 0:
                    log.add(guard.name, _summary(output) or "%s failed" % guard.name)
                log.write(part)
        if sarif_path:
            sarif.merge([part for part in parts if part.exists()], sarif_path)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return failed


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Run the guards that need no build, and exit 1 if any fails. "
                    "Every guard judges only what changed since the base, unless --all.")
    parser.add_argument("--sarif", metavar="FILE",
                        help="write one SARIF file with one run per guard")
    changed_lines.add_arguments(parser)
    parser.add_argument("--files", nargs="+", metavar="PATH",
                        help="files or directories for the header rules guard only, "
                             "limited to their changed lines unless --all")
    args = parser.parse_args(argv)
    if args.all:
        scope = ["--all"]
        change = None
    else:
        base, tried = changed_lines.resolve_base(REPO, args.base)
        if base is None:
            print("lint: no base found, tried %s; use --base REF or --all" % ", ".join(tried))
            return 1
        scope = ["--base", base]
        change = changed_lines.Change(REPO, base)
    skipped = []
    failed = run_guards(GUARDS, args.sarif, args.files, scope, skipped, change=change)
    did_not_run = ""
    if skipped:
        did_not_run = "%d guard%s did not run: %s" % (
            len(skipped), "" if len(skipped) == 1 else "s", ", ".join(skipped))
    if failed:
        print("\nlint: failed: %s%s" % (", ".join(failed), "; " + did_not_run if did_not_run else ""))
        return 1
    if did_not_run:
        print("\nlint: ok, but %s" % did_not_run)
        return 0
    print("\nlint: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
