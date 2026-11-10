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

"""Find the files and lines a change touched, for the incremental guards.

`lint.py`, `format.py` and the length and title guards judge only what the
branch changed. The change is everything since the merge base of HEAD with the
base ref: committed work, staged and unstaged edits, and untracked files that
git does not ignore, which count as all new. A file renamed from the base is
compared with its old path, so only the lines the rename edits count. The base
ref is `--base REF`, else the default branch of `upstream`, else
`upstream/develop`, else `origin/develop`.

`add_arguments` gives a script the `--base REF` and `--all` options, and
`from_args` turns them into a `Change`, or into None when `--all` asks for the
whole tree. `blamed_lines` blames the lines a rewrite changes on the lines the
change touched, for the guards that rewrite a file and compare.
"""

import difflib
import re
import subprocess
from pathlib import Path


class NoBase(Exception):
    """No base ref resolved; `tried` lists the refs looked at."""

    def __init__(self, tried):
        super().__init__("no base found, tried %s" % ", ".join(tried))
        self.tried = tried


def git(repo, *args):
    """Return the standard output of a git command in `repo`, or None if it fails."""
    done = subprocess.run(["git", "-c", "color.ui=never", "-c", "core.quotePath=false", *args], cwd=str(repo), stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL, text=True)
    return done.stdout if done.returncode == 0 else None


def resolve_base(repo, override=None):
    """Return `(commit, tried)`: the merge base of HEAD with the base ref.

    `tried` lists the refs looked at; `commit` is None when none resolved.
    """
    if override:
        candidates = [override]
    else:
        candidates = []
        head = git(repo, "symbolic-ref", "-q", "refs/remotes/upstream/HEAD")
        if head and head.strip():
            candidates.append(head.strip())
        candidates += ["upstream/develop", "origin/develop"]
    for ref in candidates:
        base = git(repo, "merge-base", "HEAD", ref)
        if base and base.strip():
            return base.strip(), candidates
    return None, candidates


def split_lines(text, keepends=False):
    """Split `text` into lines the way git counts them: on `\\n` only.

    `str.splitlines` also splits on form feeds, vertical tabs, the separators
    \\x1c to \\x1e, \\x85, U+2028 and U+2029, and on a lone `\\r`, so its line
    numbers drift from the ones in a diff. A last line without a newline still
    counts, and `\\r` stays at the end of its line.
    """
    lines = text.split("\n")
    last = lines.pop()
    if keepends:
        lines = [line + "\n" for line in lines]
    if last:
        lines.append(last)
    return lines


def _count_lines(data):
    """Return the number of lines git counts in the bytes `data`."""
    return data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)


def _parse_diff(output):
    """Return `(new_paths, ranges, renamed)` from `git diff -M -U0 --raw -p` output.

    `new_paths` holds the paths of the files the change added, copied or
    renamed; `ranges` maps each path to its added or modified `(first, last)`
    line ranges; `renamed` maps a renamed path to the list holding its old path.
    """
    new_paths = set()
    ranges = {}
    renamed = {}
    current = None
    in_header = False
    for line in output.split("\n"):
        if line.startswith("diff --git "):
            current = None
            in_header = True
        elif in_header and line.startswith("+++ "):
            name = line[4:].split("\t")[0]
            current = name[2:] if name.startswith("b/") else None
            if current is not None:
                ranges.setdefault(current, [])
        elif line.startswith("@@"):
            in_header = False
            match = re.match(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", line)
            if match and current is not None:
                start = int(match.group(1))
                length = 1 if match.group(2) is None else int(match.group(2))
                if length:
                    ranges[current].append((start, start + length - 1))
        elif not in_header and current is None and line.startswith(":"):
            head, _, names = line.partition("\t")
            status = head.split(" ")[-1][:1]
            fields = names.split("\t")
            if status == "A" and len(fields) == 1:
                new_paths.add(fields[0])
            elif status in ("R", "C") and len(fields) == 2:
                new_paths.add(fields[1])
                if status == "R":
                    renamed[fields[1]] = [fields[0]]
    return new_paths, ranges, renamed


class Change:
    """The files and lines the working tree changed since `base`.

    One `git diff` and one `git ls-files` answer every question, the first time
    one is asked.
    """

    def __init__(self, repo, base):
        self.repo = Path(repo)
        self.base = base
        self._loaded = False
        self._files = []
        self._names = None
        self._untracked = set()
        self._new_paths = set()
        self._ranges = {}
        self._renamed = {}

    def _load(self):
        if self._loaded:
            return
        self._loaded = True
        diff = git(self.repo, "diff", "-M", "--no-color", "--no-ext-diff", "-U0", "--raw", "-p", self.base) or ""
        self._new_paths, self._ranges, self._renamed = _parse_diff(diff)
        raw = [line for line in diff.split("\n") if line.startswith(":")]
        names = set()
        for line in raw:
            names.add(line.split("\t")[-1])
        others = git(self.repo, "ls-files", "--others", "--exclude-standard") or ""
        self._untracked = {line for line in others.split("\n") if line}
        names |= self._untracked
        self._files = sorted(name for name in names if (self.repo / name).is_file())

    def files(self):
        """Return the sorted repo-relative posix paths of the changed files that still exist."""
        self._load()
        return self._files

    def paths(self):
        """Return the sorted repo-relative posix paths the change touched, deleted files and the old paths of renames included.

        It runs one cheap `git diff --name-only`, not the full patch.
        """
        if self._names is None:
            out = git(self.repo, "diff", "--no-renames", "--name-only", "-z", self.base) or ""
            others = git(self.repo, "ls-files", "-z", "--others", "--exclude-standard") or ""
            self._names = sorted({n for n in (out + "\0" + others).split("\0") if n})
        return self._names

    def base_files(self):
        """Return the sorted repo-relative posix paths of the files that existed at the base."""
        out = git(self.repo, "ls-tree", "-r", "-z", "--name-only", self.base) or ""
        return sorted(name for name in out.split("\0") if name)

    def is_new_path(self, relative):
        """Return whether `relative` did not exist at the base: an added, renamed or untracked file."""
        self._load()
        return relative in self._untracked or relative in self._new_paths

    def ranges(self, relative):
        """Return the `(first, last)` ranges of the lines of `relative` the change touched.

        A file renamed from the base counts only the lines the rename edits. An
        untracked file is all new.
        """
        self._load()
        if relative in self._untracked:
            count = _count_lines((self.repo / relative).read_bytes())
            return [(1, max(count, 1))]
        return self._ranges.get(relative, [])

    def lines(self, relative):
        """Return the set of 1-based lines of `relative` the change touched."""
        return {line for first, last in self.ranges(relative) for line in range(first, last + 1)}


def add_arguments(parser):
    """Add the `--base REF` and `--all` options to `parser`."""
    parser.add_argument("--base", metavar="REF",
                        help="judge what changed since the merge base of HEAD with REF "
                             "(default: the default branch of upstream, else "
                             "upstream/develop, else origin/develop)")
    parser.add_argument("--all", action="store_true",
                        help="judge the whole tree instead of what changed")


def from_args(args, repo):
    """Return the `Change` the options ask for, or None for `--all`.

    Raises `NoBase` when no base ref resolves and `--all` is not given.
    """
    if args.all:
        return None
    base, tried = resolve_base(repo, args.base)
    if base is None:
        raise NoBase(tried)
    return Change(repo, base)


def _align(old, new, keep_old):
    """Return the pairs `(i, j)` of a longest common subsequence of `old` and `new`.

    Among the longest, it keeps the most lines `i` for which `keep_old(i)` is
    true, so that a line the change did not touch stays in place whenever the
    rewrite could equally move it or the line beside it.
    """
    if len(old) * len(new) > 250000:
        matcher = difflib.SequenceMatcher(None, old, new, autojunk=False)
        return [(block.a + k, block.b + k)
                for block in matcher.get_matching_blocks() for k in range(block.size)]
    rows, cols = len(old), len(new)
    score = [[0] * (cols + 1) for _ in range(rows + 1)]
    for i in range(rows - 1, -1, -1):
        for j in range(cols - 1, -1, -1):
            best = max(score[i + 1][j], score[i][j + 1])
            if old[i] == new[j]:
                best = max(best, score[i + 1][j + 1] + 1000 + (1 if keep_old(i) else 0))
            score[i][j] = best
    pairs = []
    i = j = 0
    while i < rows and j < cols:
        gain = 1000 + (1 if keep_old(i) else 0)
        if old[i] == new[j] and score[i][j] == score[i + 1][j + 1] + gain:
            pairs.append((i, j))
            i += 1
            j += 1
        elif score[i][j] == score[i + 1][j]:
            i += 1
        else:
            j += 1
    return pairs


def blamed_lines(old, new, touched):
    """Return the sorted 1-based lines of `old` that the rewrite `old` -> `new` is blamed on.

    A line the rewrite changes or removes is blamed on itself. A line it only
    moves is blamed on the line that moves, never on the place it lands, and
    when either of two lines could be the one that moved, on a line in
    `touched` (the lines the change added or modified; None means all). An
    older include that is out of place and gets moved past a new include in
    its right place therefore never fails the new line. A line the rewrite
    adds is blamed on the line it lands before.
    """
    head = 0
    while head < min(len(old), len(new)) and old[head] == new[head]:
        head += 1
    tail = 0
    while tail < min(len(old), len(new)) - head and old[-1 - tail] == new[-1 - tail]:
        tail += 1
    a, b = old[head:len(old) - tail], new[head:len(new) - tail]
    if not a and not b:
        return []
    pairs = _align(a, b, lambda i: touched is not None and (head + i + 1) not in touched)
    kept_a = {i for i, _ in pairs}
    kept_b = {j for _, j in pairs}
    left = {}
    for i in range(len(a)):
        if i not in kept_a:
            left[a[i]] = left.get(a[i], 0) + 1
    hit = {head + i + 1 for i in range(len(a)) if i not in kept_a}
    partner = {j: i for i, j in pairs}
    next_kept = {}
    following = len(a)
    for j in range(len(b) - 1, -1, -1):
        next_kept[j] = following
        if j in kept_b:
            following = partner[j]
    for j in range(len(b)):
        if j in kept_b:
            continue
        if left.get(b[j], 0) > 0:
            left[b[j]] -= 1
            continue
        if head + next_kept[j] in hit:
            continue
        hit.add(min(head + next_kept[j] + 1, max(len(old), 1)))
    return sorted(hit)
