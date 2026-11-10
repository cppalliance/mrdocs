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

"""The two naming rules of the C++ headers, shared by the check and the formatter.

A header guard matches the path of its header (`MRDOCS_API_` for
include/mrdocs, `MRDOCS_LIB_` for src/mrdocs, `MRDOCS_TOOL_` for tools/mrdocs,
`MRDOCS_TEST_` for tests and `MRDOCS_TEST_SUITE_` for libs/test_suite), and a
public header is included as `<mrdocs/...>`. `apply` finds, and with
`write=True` fixes, the lines of one file that break a rule. `collect_work`
lists the files and lines to judge. `check_header_rules.py` only reports;
`utils/format/format.py` imports this module to fix the same lines.
"""

import os
import re
from typing import List, NamedTuple, Optional, Set, Tuple

import changed_lines
import sarif

PROCESS_EXTS = {".hpp", ".cpp"}
# Trees under the repository root that hold inputs rather than project code: the
# golden test fixtures are sources the tests parse, so their text must not change.
# tests/integration/cmake is a standalone consumer project with its own
# public headers and guards, not MrDocs code.
SKIPPED_TREES = ("tests/golden/fixtures", "tests/integration/cmake")
RULE_HEADER = "header-rules"


class Finding(NamedTuple):
    line: int
    rule: str
    message: str


# ---------- guard name ----------
def make_guard(abs_path: str, base: str, guard_prefix: str) -> str:
    rel = os.path.relpath(abs_path, base).replace(os.sep, "/")
    suffix = re.sub(r'[^0-9A-Za-z]+', '_', rel).upper().strip('_')
    guard = f"{guard_prefix}{suffix}" if guard_prefix else suffix
    if not re.match(r'[A-Z_]', guard[0]):
        guard = f"_{guard}"
    return guard

# ---------- comment handling ----------
def strip_line_comments_and_blocks(line: str, in_block: bool) -> Tuple[str, bool]:
    i, n = 0, len(line)
    out = []
    while i < n:
        if in_block:
            end = line.find("*/", i)
            if end == -1:
                return "".join(out), True
            i = end + 2
            in_block = False
        else:
            if line.startswith("/*", i):
                in_block = True
                i += 2
                continue
            if line.startswith("//", i):
                break
            out.append(line[i])
            i += 1
    return "".join(out), in_block

def iter_significant(lines: List[str]) -> List[Tuple[int, str]]:
    sig = []
    in_block = False
    for idx, raw in enumerate(lines):
        text, in_block = strip_line_comments_and_blocks(raw, in_block)
        s = text.strip()
        if s:
            sig.append((idx, s))
    return sig

# ---------- guard structure detection ----------
RE_IFNDEF = re.compile(r'^#\s*ifndef\s+([A-Za-z_][A-Za-z0-9_]*)\s*$')
def match_define(s: str, name: str) -> bool:
    return re.match(rf'^#\s*define\s+{re.escape(name)}(\s+.*)?$', s) is not None

def detect_guard_structure(lines: List[str]) -> Optional[Tuple[int, str, int, int]]:
    """
    Returns (ifndef_idx, name, define_idx, endif_idx) iff:
    - first significant token is '#ifndef NAME'
    - next significant token is '#define NAME'
    - last significant token is '#endif'
    """
    sig = iter_significant(lines)
    if not sig:
        return None
    m = RE_IFNDEF.match(sig[0][1])
    if not m:
        return None
    name = m.group(1)
    if len(sig) < 2 or not match_define(sig[1][1], name):
        return None
    if not sig[-1][1].startswith("#endif"):
        return None
    return sig[0][0], name, sig[1][0], sig[-1][0]

# ---------- misc utils ----------
def detect_eol(s: str) -> str:
    if s.endswith("\r\n"): return "\r\n"
    if s.endswith("\n"):   return "\n"
    if s.endswith("\r"):   return "\r"
    return ""

def leading_ws(s: str) -> str:
    m = re.match(r'^[ \t]*', s)
    return m.group(0) if m else ""

def _normalize_slashes(p: str) -> str:
    return p.replace("\\", "/")

def _is_path_within(child: str, parent: str) -> bool:
    try:
        child = os.path.realpath(child)
        parent = os.path.realpath(parent)
        return os.path.commonpath([child, parent]) == parent
    except Exception:
        return False

# ---------- include handling ----------
INCLUDE_RE = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)([>"])(.*?)(\r?\n|\r)?$')

def _resolve_from(token: str, root: str) -> Optional[str]:
    p = os.path.normpath(os.path.join(root, token))
    return p if os.path.isfile(p) else None

def _same_dir_exists(token: str, cur_dir: str) -> bool:
    p = os.path.normpath(os.path.join(cur_dir, token))
    return os.path.isfile(p) and (os.path.dirname(p) == os.path.normpath(cur_dir))

def _rewrite_includes(lines: List[str], path: str, include_root: str,
                      touched: Optional[Set[int]] = None) -> List[Tuple[int, str]]:
    """
    Normalize #include directives for the public/private layout (no -I src):

      * Public headers (resolve under 'include_root', i.e. include/) are always
        angle-bracketed and spelled relative to include_root:
        #include <mrdocs/Foo.hpp>.
      * Private headers (resolve relative to the including file's directory, in
        the same dir or a subdir of the same target, or via a relative path to
        another target) stay quoted, exactly as written. There is no -I src, so
        these must remain "..." relative includes.
      * Everything else (system/external, e.g. <llvm/...>, <test_suite/...>) is
        left untouched.

    The same rule applies to .hpp and .cpp. Only the lines in 'touched' (1-based;
    None means every line) are rewritten. Returns the (line, new spelling) of
    each line it rewrote.
    """
    rewritten: List[Tuple[int, str]] = []
    in_block = False
    cur_dir = os.path.dirname(path)

    for i, raw in enumerate(lines):
        visible, in_block = strip_line_comments_and_blocks(raw, in_block)
        if in_block:
            continue
        if visible.strip().startswith("//"):
            continue
        if touched is not None and (i + 1) not in touched:
            continue

        m = INCLUDE_RE.match(raw)
        if not m:
            continue

        prefix, open_ch, token, close_ch, tail, eol = m.groups()
        eol = eol or ""
        token_stripped = token.strip()

        cur_abs = _resolve_from(token_stripped, cur_dir)
        root_abs = _resolve_from(token_stripped, include_root)

        spelled = None
        if cur_abs and _is_path_within(cur_abs, include_root):
            # Public header reached via a relative path: normalize to <mrdocs/...>.
            spelled = _normalize_slashes(os.path.relpath(cur_abs, include_root))
        elif cur_abs:
            # Private header relative to this file: keep it quoted, as written.
            pass
        elif root_abs and _is_path_within(root_abs, include_root):
            # Public header, possibly mis-quoted: normalize to <mrdocs/...>.
            spelled = _normalize_slashes(os.path.relpath(root_abs, include_root))
        # Otherwise system/external or already-correct: leave untouched.

        if spelled is not None:
            new_line = f"{prefix}<{spelled}>{tail}{eol}"
            if new_line != raw:
                lines[i] = new_line
                rewritten.append((i + 1, spelled))

    return rewritten

# ---------- walk requested trees ----------
def _is_skipped(path: str, repo_root: str) -> bool:
    """True when 'path' lies in one of the SKIPPED_TREES."""
    rel = os.path.relpath(os.path.realpath(path), os.path.realpath(repo_root)).replace(os.sep, "/")
    return any(rel == tree or rel.startswith(tree + "/") for tree in SKIPPED_TREES)

def default_targets(repo_root: str) -> List[Tuple[str, str, str]]:
    """(guard_base, guard_prefix, include_root) for each default tree."""
    # The only public include root is include/. Private headers live beside
    # their sources and are included via quoted relative paths (no -I src), so
    # every target resolves "public -> angle" against include/.
    inc = os.path.join(repo_root, "include")
    return [
        (os.path.join(repo_root, "include", "mrdocs"),  "MRDOCS_API_",        inc),
        (os.path.join(repo_root, "src", "mrdocs"),      "MRDOCS_LIB_",        inc),
        (os.path.join(repo_root, "tools", "mrdocs"),    "MRDOCS_TOOL_",       inc),
        (os.path.join(repo_root, "tests"),              "MRDOCS_TEST_",       inc),
        (os.path.join(repo_root, "libs", "test_suite"), "MRDOCS_TEST_SUITE_", inc),
    ]

def _list_files(path: str) -> List[str]:
    if os.path.isfile(path):
        return [path]
    out = []
    for root, _, files in os.walk(path):
        for fn in sorted(files):
            if os.path.splitext(fn)[1].lower() in PROCESS_EXTS:
                out.append(os.path.join(root, fn))
    return sorted(out)

def _target_for(path: str, targets) -> Tuple[Optional[str], str, str]:
    """Pick the target that owns 'path'; (None, "", include_root) when none does."""
    for base, prefix, inc_root in targets:
        if _is_path_within(path, base):
            return base, prefix, inc_root
    return None, "", targets[0][2]


def apply(lines: List[str], path: str, guard_base: Optional[str], guard_prefix: str,
          include_root: str, touched: Optional[Set[int]] = None,
          path_changed: bool = False) -> Tuple[List[Finding], List[str]]:
    """
    Fix 'lines' in place and return (findings, steps).

    1) For .hpp: if the canonical guard differs from the guard, rewrite it
       (only when one of its three lines is in 'touched', or when the file's
       path is new, so the guard has to match the path it now has).
    2) For .hpp & .cpp: normalize the includes on the lines in 'touched'.

    'touched' holds the 1-based lines the change touched; None means every line.
    A guard_base of None means the file is outside the known trees: its guard is left alone.
    The findings hold one entry per line rewritten.
    """
    is_header = os.path.splitext(path)[1].lower() == ".hpp"
    findings: List[Finding] = []
    steps: List[str] = []

    if is_header and guard_base is not None:
        structure = detect_guard_structure(lines)
        if structure is not None:
            ifndef_idx, old_name, define_idx, endif_idx = structure
            new_name = make_guard(path, guard_base, guard_prefix)
            owned = {ifndef_idx + 1, define_idx + 1, endif_idx + 1}
            if old_name != new_name and (touched is None or path_changed or owned & touched):
                for idx, template in ((ifndef_idx, "#ifndef {}"), (define_idx, "#define {}"),
                                      (endif_idx, "#endif // {}")):
                    new_line = f"{leading_ws(lines[idx])}{template.format(new_name)}{detect_eol(lines[idx])}"
                    if lines[idx] != new_line:
                        lines[idx] = new_line
                        findings.append(Finding(
                            idx + 1, RULE_HEADER,
                            f"include guard {old_name} would become {new_name}"))
                if findings:
                    steps.append(f"guard {old_name} -> {new_name}")

    for line, spelled in _rewrite_includes(lines, path, include_root, touched):
        findings.append(Finding(line, RULE_HEADER,
                                f"include would be spelled <{spelled}>"))
        if "includes normalized" not in steps:
            steps.append("includes normalized")
    return findings, steps


Work = Tuple[str, Optional[str], str, str, Optional[Set[int]], bool]


def collect_work(paths: List[str], repo_root: str,
                 change: Optional[changed_lines.Change] = None) -> Tuple[List[Work], int]:
    """
    List the files to judge as (file, guard base, guard prefix, include root,
    touched lines or None, path is new), and the exit status to stop with
    (0 to go on, 2 when a named path does not exist).
    'paths' names files or directories; with none, the files the change
    touched, or every file of the five trees when 'change' is None.
    """
    targets = default_targets(repo_root)
    work: List[Work] = []

    def touched_in(f: str) -> Optional[Set[int]]:
        if change is None:
            return None
        rel = os.path.relpath(f, repo_root).replace(os.sep, "/")
        if rel not in set(change.files()):
            return set()
        return change.lines(rel)

    def moved_in(f: str) -> bool:
        if change is None:
            return False
        return change.is_new_path(os.path.relpath(f, repo_root).replace(os.sep, "/"))

    if paths:
        for p in paths:
            if not os.path.exists(p):
                print(f"[error] {p}: no such file or directory")
                return work, 2
            if _is_skipped(p, repo_root):
                print(f"[skip] {p} (in {', '.join(SKIPPED_TREES)})")
                continue
            for f in _list_files(p):
                if os.path.splitext(f)[1].lower() not in PROCESS_EXTS:
                    print(f"[skip] {f} (not a {'/'.join(sorted(PROCESS_EXTS))} file)")
                    continue
                if _is_skipped(f, repo_root):
                    continue
                work.append((f, *_target_for(f, targets), touched_in(f), moved_in(f)))
    elif change is None:
        for base, prefix, inc_root in targets:
            if not os.path.isdir(base):
                print(f"[skip dir] {base} (not found)")
                continue
            for f in _list_files(base):
                if not _is_skipped(f, repo_root):
                    work.append((f, base, prefix, inc_root, None, False))
    else:
        for rel in change.files():
            if os.path.splitext(rel)[1].lower() not in PROCESS_EXTS:
                continue
            f = os.path.join(repo_root, rel)
            base, prefix, inc_root = _target_for(f, targets)
            if base is None or _is_skipped(f, repo_root):
                continue
            work.append((f, base, prefix, inc_root, change.lines(rel), change.is_new_path(rel)))
    return work, 0


def check(paths: List[str], repo_root: str, sarif_path: Optional[str] = None,
          change: Optional[changed_lines.Change] = None) -> int:
    """
    Print each line that breaks a rule as `file:line: message (header-rules)`
    and return 1 when any does, 2 when a named path is missing, else 0.
    Nothing is written. 'change' limits the lines judged; None means every line.
    """
    work, status = collect_work(paths, repo_root, change)
    if status:
        return status
    log = sarif.Log("header-rules", [RULE_HEADER])
    total = failing = skipped = 0
    for f, base, prefix, inc_root, touched, new_path in work:
        total += 1
        rel = os.path.relpath(f, repo_root).replace(os.sep, "/")
        try:
            with open(f, "r", encoding="utf-8-sig", newline="") as handle:
                lines = changed_lines.split_lines(handle.read(), keepends=True)
            if not lines or (touched is not None and not touched and not new_path):
                continue
            findings, _ = apply(lines, f, base, prefix, inc_root, touched, new_path)
        except Exception as error:
            skipped += 1
            print(f"{rel}: skipped (error: {error})")
            log.add(RULE_HEADER, f"skipped (error: {error})", uri=rel)
            continue
        if findings:
            failing += 1
        for finding in sorted(set(findings), key=lambda x: (x.line, x.rule)):
            print(f"{rel}:{finding.line}: {finding.message} ({finding.rule})")
            log.add(finding.rule, finding.message, uri=rel, start_line=finding.line)
    print(f"\nAll done. {total} files total, {failing} would change, {skipped} skipped.")
    if sarif_path:
        log.write(sarif_path)
    return 1 if failing or skipped else 0
