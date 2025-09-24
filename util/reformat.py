#!/usr/bin/env python3
import os
import re
import subprocess
import shutil
from typing import List, Optional, Tuple

# ---------- configuration ----------
PROCESS_EXTS = {".hpp", ".cpp"}  # extend as needed
CLANG_FORMAT_EXE = shutil.which("clang-format")  # resolve once

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

def _detect_include_blocks(lines: List[str]) -> List[Tuple[int, int]]:
    blocks: List[Tuple[int, int]] = []
    in_block_comment = False
    block_start: Optional[int] = None
    last_include: Optional[int] = None

    def is_blank_or_comment(i: int) -> bool:
        nonlocal in_block_comment
        visible, in_block_comment = strip_line_comments_and_blocks(lines[i], in_block_comment)
        s = visible.strip()
        return (s == "" or s.startswith("//"))

    for i in range(len(lines)):
        raw = lines[i]
        visible, in_block_comment = strip_line_comments_and_blocks(raw, in_block_comment)
        if in_block_comment:
            continue

        is_inc = INCLUDE_RE.match(raw) is not None
        if is_inc:
            if block_start is None:
                block_start = i
            last_include = i
            continue

        if block_start is not None:
            if is_blank_or_comment(i):
                continue
            blocks.append((block_start + 1, last_include + 1))
            block_start = None
            last_include = None

    if block_start is not None and last_include is not None:
        blocks.append((block_start + 1, last_include + 1))

    # merge adjacent
    merged: List[Tuple[int, int]] = []
    for s, e in blocks:
        if not merged or s > merged[-1][1] + 1:
            merged.append((s, e))
        else:
            merged[-1] = (merged[-1][0], max(merged[-1][1], e))
    return merged

def _clang_format_ranges_if_changed(path: str, ranges: List[Tuple[int, int]]) -> str:
    """
    Run clang-format once on multiple ranges, but only report 'changed'
    if file content actually changed.
    Returns: 'changed' | 'unchanged' | 'not-found' | 'error: ...' | 'no-op'
    """
    if not ranges:
        return "no-op"
    if CLANG_FORMAT_EXE is None:
        return "not-found"

    with open(path, "rb") as f:
        before = f.read()

    cmd = [CLANG_FORMAT_EXE, "-i", "-style", "file", "--sort-includes"]
    for s, e in ranges:
        cmd.append(f"-lines={s}:{e}")
    cmd.append(path)

    try:
        subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        return f"error: clang-format failed: {e}"

    with open(path, "rb") as f:
        after = f.read()

    return "changed" if after != before else "unchanged"

def _rewrite_includes(lines: List[str], path: str, include_root: str, is_header: bool) -> bool:
    """
    Normalize #include directives:

      - Headers (.hpp):
          * Any quoted include becomes angle brackets.
          * If the header resolves under 'include_root' (either from the current
            directory or directly under include_root), rewrite token to the path
            relative to 'include_root':  #include <REL/FROM/INCLUDE_ROOT.hpp>
            (POSIX slashes). Example: src/lib header "ConfigImpl.hpp" -> <lib/ConfigImpl.hpp>.
          * Otherwise, just flip quotes to angles without touching the token.
      - .cpp:
          * Quoted -> angle brackets ONLY if target is NOT same-dir (exact FS check).
          * Same-dir quoted includes are left as-is.
          * Never modify the token content for .cpp.
    """
    changed = False
    in_block = False
    cur_dir = os.path.dirname(path)

    for i, raw in enumerate(lines):
        visible, in_block = strip_line_comments_and_blocks(raw, in_block)
        if in_block:
            continue
        if visible.strip().startswith("//"):
            continue

        m = INCLUDE_RE.match(raw)
        if not m:
            continue

        prefix, open_ch, token, close_ch, tail, eol = m.groups()
        eol = eol or ""
        token_stripped = token.strip()

        if is_header:
            # Try to resolve under include_root. Prefer resolution from cur_dir if it exists,
            # but the final output is always include_root-relative.
            target_abs = None
            cur_abs = _resolve_from(token_stripped, cur_dir)
            if cur_abs and _is_path_within(cur_abs, include_root):
                target_abs = cur_abs
            else:
                root_abs = _resolve_from(token_stripped, include_root)
                if root_abs and _is_path_within(root_abs, include_root):
                    target_abs = root_abs

            if target_abs and _is_path_within(target_abs, include_root):
                rel_from_root = _normalize_slashes(os.path.relpath(target_abs, include_root))
                new_line = f"{prefix}<{rel_from_root}>{tail}{eol}"
            else:
                # Could be system/external. Still convert quotes to angles per rule.
                if open_ch == '"' and close_ch == '"':
                    new_line = f"{prefix}<{token_stripped}>{tail}{eol}"
                else:
                    new_line = raw  # already angled; leave token as-is

            if new_line != raw:
                lines[i] = new_line
                changed = True
            continue

        # .cpp policy
        if _same_dir_exists(token_stripped, cur_dir):
            continue  # keep same-dir quoted includes
        if open_ch == '"' and close_ch == '"':
            new_line = f"{prefix}<{token_stripped}>{tail}{eol}"
            if new_line != raw:
                lines[i] = new_line
                changed = True

    return changed

# ---------- per-file transform ----------
def process_file(path: str, guard_base: str, guard_prefix: str, include_root: str) -> str:
    """
    1) For .hpp: If canonical guard exists AND name differs, update; otherwise leave guard as-is.
    2) For .hpp & .cpp: normalize includes per rules (using include_root).
    3) clang-format: run once per file on all include blocks; only report if bytes changed.
    """
    ext = os.path.splitext(path)[1].lower()
    is_header = (ext == ".hpp")

    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        lines = f.readlines()
    if not lines:
        return "empty"

    changed = False
    guard_changed = False
    guard_msg = None

    # Optional guard canonicalization (headers only) — only if new name differs
    if is_header:
        structure = detect_guard_structure(lines)
        if structure is not None:
            ifndef_idx, old_name, define_idx, endif_idx = structure
            new_name = make_guard(path, guard_base, guard_prefix)
            if old_name != new_name:
                eol = detect_eol(lines[ifndef_idx]); pre = leading_ws(lines[ifndef_idx])
                new_ifndef = f"{pre}#ifndef {new_name}{eol}"
                if lines[ifndef_idx] != new_ifndef:
                    lines[ifndef_idx] = new_ifndef; changed = True; guard_changed = True

                eol = detect_eol(lines[define_idx]); pre = leading_ws(lines[define_idx])
                new_define = f"{pre}#define {new_name}{eol}"
                if lines[define_idx] != new_define:
                    lines[define_idx] = new_define; changed = True; guard_changed = True

                eol = detect_eol(lines[endif_idx]); pre = leading_ws(lines[endif_idx])
                new_endif = f"{pre}#endif // {new_name}{eol}"
                if lines[endif_idx] != new_endif:
                    lines[endif_idx] = new_endif; changed = True; guard_changed = True

                if guard_changed:
                    guard_msg = f"guard {old_name} -> {new_name}"

    # Include normalization across whole file
    inc_changed = _rewrite_includes(lines, path, include_root, is_header)
    if inc_changed:
        changed = True

    # If buffer changed, write prior to clang-format
    if changed:
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.writelines(lines)

    # clang-format once per file; only report if bytes changed
    blocks = _detect_include_blocks(lines)
    fmt_status = _clang_format_ranges_if_changed(path, blocks)
    fmt_note = ""
    fmt_changed = (fmt_status == "changed")
    if fmt_status == "not-found":
        fmt_note = " (clang-format not found)"
    elif fmt_status.startswith("error"):
        fmt_note = f" ({fmt_status})"

    # Reload if clang-format changed the file
    if fmt_changed:
        with open(path, "r", encoding="utf-8-sig", newline="") as f:
            lines = f.readlines()

    # Status message
    parts = []
    if guard_changed and guard_msg:
        parts.append(guard_msg)
    if inc_changed:
        parts.append("includes normalized")
    if fmt_changed:
        parts.append("include blocks clang-formatted")

    if parts:
        return "updated (" + "; ".join(parts) + ")" + fmt_note
    return "ok" + fmt_note

# ---------- walk requested trees ----------
def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    include_root_public = os.path.normpath(os.path.join(script_dir, "..", "include"))
    include_root_src    = os.path.normpath(os.path.join(script_dir, "..", "src"))

    targets = [
        # (guard_base, guard_prefix, include_root)
        (os.path.join(script_dir, "..", "include", "mrdocs"), "MRDOCS_API_",        include_root_public),
        (os.path.join(script_dir, "..", "src", "lib"),        "MRDOCS_LIB_",        include_root_src),
        (os.path.join(script_dir, "..", "src", "test"),       "MRDOCS_TEST_",       include_root_src),
        (os.path.join(script_dir, "..", "src", "test_suite"), "MRDOCS_TEST_SUITE_", include_root_src),
        (os.path.join(script_dir, "..", "src", "tool"),       "MRDOCS_TOOL_",       include_root_src),
    ]

    grand_total = grand_updated = grand_skipped = 0

    for base, prefix, inc_root in targets:
        if not os.path.isdir(base):
            print(f"[skip dir] {base} (not found)")
            continue

        total = updated = skipped = 0
        print(f"\n== Processing: {base}  (prefix: {prefix}; include_root: {inc_root}) ==")
        for root, _, files in os.walk(base):
            for fn in files:
                ext = os.path.splitext(fn)[1].lower()
                if ext not in PROCESS_EXTS:
                    continue
                total += 1
                path = os.path.join(root, fn)
                try:
                    result = process_file(path, base, prefix, inc_root)
                except Exception as e:
                    result = f"skipped (error: {e})"
                if result.startswith("updated"):
                    updated += 1
                elif result.startswith("skipped"):
                    skipped += 1
                print(f"{os.path.relpath(path, base)}: {result}")

        grand_total += total
        grand_updated += updated
        grand_skipped += skipped
        print(f"-- Dir summary: {total} files, {updated} updated, {skipped} skipped.")

    print(f"\nAll done. {grand_total} files total, {grand_updated} updated, {grand_skipped} skipped.")

if __name__ == "__main__":
    main()
