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

"""Check the top-level entries against the lists in this file.

`ALLOWED_DIRS` lists the allowed directories, `ALLOWED_FILES` the allowed
files and `TOLERATED` the tracked entries the project tolerates. A trailing
slash marks a directory. The lists are the source of truth.

The page docs/modules/ROOT/pages/contribute/codebase-layout.adoc shows the same
three lists as tables, marked by the anchors `layout-allow-list`,
`layout-allow-files` and `layout-tolerated`. A table row starts with `|`, and
the first cell names the entry between backticks, either alone
(`CMakeLists.txt`) or as the text of an xref
(`xref:contribute/layout/src.adoc[`src/`]`). The check fails when a table names
an entry the lists lack, lacks one they have, names an entry twice or has a
row without a code span. An entry in more than one list, or twice in one list,
fails too.

Every rule reads one file list: the tracked files plus the untracked files
that are not ignored, minus the files deleted from the working tree. A new
page counts before it is committed, and the local run agrees with CI.

Each allowed directory also needs its page, `<LAYOUT_DIR>/<dir>.adoc`, and no
page may exist for a name that is not allowed. A `README.md` (any case) must not
exist at `<dir>/` for an allowed directory, nor at `include/mrdocs/` or
`src/mrdocs/`, because the layout pages are the source of those rules.

A C++ project uses the root layout: its top level holds only the directories of
`ALLOWED_DIRS`, the files of `ALLOWED_FILES` and a `CMakeLists.txt`, and no
`README.md`. `CPP_PROJECTS` lists the project roots, `libs/*` for each library
and `tools/mrdocs` for the command-line tool. The same rule applies to any
`libs/` inside a project, at any depth.

By default it judges only what the branch changed since the merge base of HEAD
with the base ref (see `changed_lines.py`): a violation counts when the tree
breaks the rule now and the base did not, so an entry the branch adds that is
not allowed fails, and an entry it removes that is required fails, while a
problem that was already there does not. The base tree is judged with the lists
this file had at the base, so a branch that edits the lists answers for the
entries it newly allows or forbids. The lists and the tables on the page
are compared when the branch edits the page or this file. `--base REF` picks
another base and `--all` judges the whole tree.

The check prints one line per violation and exits 1, or prints
`layout: ok (<n> entries)` and exits 0. With `--sarif FILE` it also writes the
violations as a SARIF file, under the rule `layout` of the layout invariants page.

    python3 -I utils/lint/check_layout.py
    python3 -I utils/lint/check_layout.py --base HEAD~3
    python3 -I utils/lint/check_layout.py --all
"""

import argparse
import ast
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import changed_lines
import sarif

RULE = "layout"
PAGE = "docs/modules/ROOT/pages/contribute/codebase-layout.adoc"
SELF = "utils/lint/check_layout.py"
LAYOUT_DIR = "docs/modules/ROOT/pages/contribute/layout"
EXTRA_README_DIRS = ("include/mrdocs", "src/mrdocs")


ALLOWED_DIRS = (
    "src/",
    "include/",
    "libs/",
    "tests/",
    "examples/",
    "docs/",
    "data/",
    "tools/",
    "utils/",
)

ALLOWED_FILES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "bootstrap.py",
    "mrdocs-config.cmake.in",
    "LICENSE.txt",
    "README.adoc",
)

TOLERATED = (
    ".github/",
    ".clang-format",
    ".gitattributes",
    ".gitignore",
)

# The C++ projects that follow the root layout. A trailing `/*` stands for every
# directory under that path, and each one is a library when the path starts with
# `libs/` and a C++ tool otherwise. No directory under `utils/` holds C++ today
# (`utils/cmake/helpers` holds two single-file sources, `HostExport.cpp` and
# `StdlibTagProbe.cpp`), so none is listed. A C++ tool added under `utils/`
# joins this list.
CPP_PROJECTS = (
    "libs/*",
    "tools/mrdocs",
)


def page_tables():
    """Return (table id, constant name, values) for each table on PAGE."""
    return (
        ("layout-allow-list", "ALLOWED_DIRS", ALLOWED_DIRS),
        ("layout-allow-files", "ALLOWED_FILES", ALLOWED_FILES),
        ("layout-tolerated", "TOLERATED", TOLERATED),
    )


def parse_table(text, table_id):
    """Return the first-cell entries of the table `table_id`, or None if absent.

    A row whose first cell has no code span yields None in its place.
    """
    pattern = r"^\[[^\]\n]*#%s[,\]][^\n]*\n\|===[ \t]*\n(.*?)^\|===[ \t]*$" % re.escape(table_id)
    match = re.search(pattern, text, re.DOTALL | re.MULTILINE)
    if not match:
        return None
    entries = []
    previous_blank = False
    header = True
    for line in match.group(1).splitlines():
        if not line.strip():
            previous_blank = True
            continue
        if line.startswith("|") and (header or previous_blank):
            if not header:
                span = re.search(r"`([^`]+)`", line)
                entries.append(span.group(1) if span else None)
            header = False
        previous_blank = False
    return entries


def check_page(repo, problem):
    """Report each entry the tables on PAGE and the lists above disagree on."""
    page = repo / PAGE
    if not page.exists():
        problem(PAGE, "%s: page not found, it must show the layout tables" % PAGE)
        return
    text = page.read_text(encoding="utf-8")
    for table_id, constant, expected in page_tables():
        rows = parse_table(text, table_id)
        if rows is None:
            problem(PAGE, "%s: table '%s' not found, it must list the entries of %s" % (PAGE, table_id, constant))
            continue
        if None in rows:
            problem(PAGE, "%s: table '%s' has a row without a code span" % (PAGE, table_id))
        rows = [row for row in rows if row is not None]
        for item in expected:
            if item not in rows:
                problem(PAGE, "%s: in %s but missing from table '%s' on %s" % (item, constant, table_id, PAGE))
        for item in dict.fromkeys(rows):
            if rows.count(item) > 1:
                problem(PAGE, "%s: listed more than once in table '%s' on %s" % (item, table_id, PAGE))
            if item not in expected:
                problem(PAGE, "%s: in table '%s' on %s but missing from %s" % (item, table_id, PAGE, constant))


def file_list(repo):
    """Return the tracked and untracked-not-ignored files that exist on disk."""
    out = subprocess.run(
        ["git", "-C", str(repo), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        check=True, capture_output=True, text=True,
    ).stdout
    return sorted({p for p in out.split("\0") if p and (repo / p).exists()})


def top_level_entries(paths):
    """Map each top-level name to True when it is a directory."""
    entries = {}
    for path in paths:
        name, slash, _ = path.partition("/")
        entries[name] = entries.get(name, False) or bool(slash)
    return entries


def project_roots(paths, parent):
    """Return (root with a trailing slash, kind) for each C++ project under `parent`.

    `parent` is "" for the repository or the root of a project. Its own `libs/*`
    entries are libraries, and the other entries of `CPP_PROJECTS` are C++ tools
    only at the repository root.
    """
    roots = {}
    for pattern in CPP_PROJECTS:
        if parent and not pattern.startswith("libs/"):
            continue
        kind = "library" if pattern.startswith("libs/") else "C++ tool"
        if pattern.endswith("/*"):
            prefix = parent + pattern[:-1]
            for p in paths:
                if p.startswith(prefix) and "/" in p[len(prefix):]:
                    roots[prefix + p[len(prefix):].partition("/")[0] + "/"] = kind
        else:
            root = parent + pattern + "/"
            if any(p.startswith(root) for p in paths):
                roots[root] = kind
    return sorted(roots.items())


def check_projects(paths, parent, allowed_dirs, allowed_files, problem):
    """Check each C++ project under `parent`, then the `libs/` inside it.

    A project holds only the directories of the directory table, the files of
    the file table and a `CMakeLists.txt`, and no `README.md`.
    """
    dir_names = {d.rstrip("/") for d in allowed_dirs}
    file_names = set(allowed_files) | {"CMakeLists.txt"}
    for root, kind in project_roots(paths, parent):
        entries = {}
        for path in paths:
            if path.startswith(root):
                name, slash, _ = path[len(root):].partition("/")
                entries[name] = entries.get(name, False) or bool(slash)
        for name, is_dir in sorted(entries.items()):
            shown = root + name + ("/" if is_dir else "")
            if name.lower() == "readme.md":
                problem(shown, "%s: not allowed, a %s has no README.md at its top" % (shown, kind))
            elif is_dir and name not in dir_names:
                problem(shown, "%s: not a directory the root layout allows in a %s" % (shown, kind))
            elif not is_dir and name not in file_names:
                problem(shown, "%s: not a file the root layout allows in a %s" % (shown, kind))
        check_projects(paths, root, allowed_dirs, allowed_files, problem)


def lists_at(change):
    """Return `(ALLOWED_DIRS, ALLOWED_FILES, TOLERATED)` as the base of `change` had them.

    The constants are read from the base copy of this file as literals, never
    executed. The current lists stand in when the base has no copy or it does
    not parse.
    """
    current = (ALLOWED_DIRS, ALLOWED_FILES, TOLERATED)
    text = changed_lines.git(change.repo, "show", "%s:%s" % (change.base, SELF))
    if text is None:
        return current
    try:
        found = {}
        for node in ast.parse(text).body:
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                found[node.targets[0].id] = tuple(ast.literal_eval(node.value))
        return tuple(found[name] for name in ("ALLOWED_DIRS", "ALLOWED_FILES", "TOLERATED"))
    except (SyntaxError, ValueError, KeyError, TypeError):
        return current


def tree_problems(paths, lists=None):
    """Return the `(uri, text)` pairs the file list `paths` breaks the layout rules with.

    `lists` is `(ALLOWED_DIRS, ALLOWED_FILES, TOLERATED)`, the current ones by default.
    """
    allowed_dirs_list, allowed_files_list, tolerated = lists or (ALLOWED_DIRS, ALLOWED_FILES, TOLERATED)
    names = list(allowed_dirs_list) + list(allowed_files_list) + list(tolerated)
    allowed = list(allowed_dirs_list) + list(allowed_files_list)
    known = {item.rstrip("/"): item.endswith("/") for item in names}
    tracked = top_level_entries(paths)
    found = []

    def problem(uri, text):
        found.append((uri, text))

    for name, is_dir in sorted(tracked.items()):
        shown = name + "/" if is_dir else name
        if name not in known:
            problem(name, "%s: tracked at the top level but not in the layout lists of %s" % (shown, SELF))
        elif known[name] != is_dir:
            kind = "a directory" if known[name] else "a file"
            problem(name, "%s: listed in the layout lists of %s as %s" % (shown, SELF, kind))
    for item in allowed:
        name = item.rstrip("/")
        if name not in tracked:
            kind = "directory" if item.endswith("/") else "file"
            problem(SELF, "%s: allowed %s is missing from the tree" % (item, kind))

    allowed_dirs = {item.rstrip("/") for item in allowed if item.endswith("/")}
    prefix = LAYOUT_DIR + "/"
    pages = {p[len(prefix):-len(".adoc")] for p in paths
             if p.startswith(prefix) and p.endswith(".adoc") and "/" not in p[len(prefix):]}
    for name in sorted(allowed_dirs - pages):
        problem("%s%s.adoc" % (prefix, name), "%s%s.adoc: missing page for the allowed directory %s/" % (prefix, name, name))
    for name in sorted(pages - allowed_dirs):
        problem("%s%s.adoc" % (prefix, name),
                "%s%s.adoc: page for a name that is not an allowed directory in %s" % (prefix, name, SELF))
    readme_dirs = sorted(allowed_dirs) + list(EXTRA_README_DIRS)
    for path in paths:
        directory, _, filename = path.rpartition("/")
        if filename.lower() == "readme.md" and directory in readme_dirs:
            problem(path, "%s: not allowed, the page %s%s.adoc is the source" % (path, prefix, directory.split("/")[0]))

    check_projects(paths, "", list(allowed_dirs_list), list(allowed_files_list), problem)
    return found


def list_problems(repo):
    """Return the `(uri, text)` pairs the layout lists and the tables on PAGE break."""
    found = []

    def problem(uri, text):
        found.append((uri, text))

    seen = set()
    for item in list(ALLOWED_DIRS) + list(ALLOWED_FILES) + list(TOLERATED):
        if item.rstrip("/") in seen:
            problem(SELF, "%s: listed more than once in the layout lists of %s" % (item, SELF))
        seen.add(item.rstrip("/"))
    check_page(repo, problem)
    return found


def main(argv=None):
    default_repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", type=Path, default=default_repo,
                        help="repository root (default: the script's repo)")
    parser.add_argument("--sarif", metavar="FILE",
                        help="also write the violations to FILE as SARIF")
    changed_lines.add_arguments(parser)
    args = parser.parse_args(argv)
    repo = args.repo
    try:
        change = changed_lines.from_args(args, repo)
    except changed_lines.NoBase as error:
        print("layout: %s; use --base REF or --all" % error)
        return 1

    paths = file_list(repo)
    found = tree_problems(paths)
    if change is None:
        found = list_problems(repo) + found
    else:
        before = {text for _, text in tree_problems(change.base_files(), lists_at(change))}
        found = [item for item in found if item[1] not in before]
        if {PAGE, SELF} & set(change.paths()):
            found = list_problems(repo) + found

    for _, text in found:
        print("layout: " + text)
    if args.sarif:
        log = sarif.Log("check_layout", [RULE])
        for uri, text in found:
            log.add(RULE, text, uri=uri)
        log.write(args.sarif)
    if found:
        return 1
    print("layout: ok (%d entries)" % len(top_level_entries(paths)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
