#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#
"""Rank C++ sources and functions by length and flag near-duplicate blocks.

This is the F1 worklist generator for the large-file / long-function
modularization effort: it tells you what to split first, longest-first, and
where copy-pasted code clusters. The function detector is a brace-matching
heuristic (no real parser), so treat its output as a worklist, not ground truth.

Usage:
  python utils/linting/code_metrics.py [--top N] [--min-func L] [--dup-window W]
                                       [--roots src include] [--md OUTFILE]
"""
import argparse, hashlib, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_EXTS = {".cpp", ".hpp", ".ipp", ".cc", ".cxx", ".h"}
# Signature-ish line that opens a definition body; excludes type/namespace openers.
SIG_RE = re.compile(r"[A-Za-z_][\w:<>,&*~\s]*\([^;]*\)\s*(const|noexcept|override|final|\s)*\{?\s*$")
SKIP_OPENERS = re.compile(r"^\s*(namespace|class|struct|union|enum|extern\s*\"C\"|template\s*<)")

def list_files(roots):
    out = []
    for r in roots:
        base = os.path.join(ROOT, r)
        for dp, _, fns in os.walk(base):
            if "third-party" in dp or "node_modules" in dp:
                continue
            for fn in fns:
                if os.path.splitext(fn)[1] in SRC_EXTS:
                    out.append(os.path.join(dp, fn))
    return out

def sanitize(text):
    """Replace string/char-literal interiors and comments with spaces, keeping
    newlines and overall length, so brace/paren counting ignores them. Without
    this, a literal like "?*[{" would corrupt brace-depth tracking."""
    out = []
    i, n = 0, len(text)
    state = None  # None | '"' | "'" | "//" | "/*"
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state is None:
            if c == "/" and nxt == "/":
                state = "//"; out.append("  "); i += 2; continue
            if c == "/" and nxt == "*":
                state = "/*"; out.append("  "); i += 2; continue
            if c == '"' or c == "'":
                state = c; out.append(c); i += 1; continue
            out.append(c); i += 1; continue
        if state == "//":
            if c == "\n":
                state = None; out.append("\n")
            else:
                out.append(" ")
            i += 1; continue
        if state == "/*":
            if c == "*" and nxt == "/":
                state = None; out.append("  "); i += 2; continue
            out.append("\n" if c == "\n" else " "); i += 1; continue
        # inside a string/char literal
        if c == "\\":
            out.append("  "); i += 2; continue
        if c == state:
            state = None; out.append(c); i += 1; continue
        out.append("\n" if c == "\n" else " "); i += 1; continue
    return "".join(out)

def code_lines(text):
    """Strip // and /* */ comments and blank lines; return (kept_line, orig_index)."""
    out, in_block = [], False
    for i, raw in enumerate(text.splitlines()):
        s = raw
        if in_block:
            end = s.find("*/")
            if end < 0:
                continue
            s, in_block = s[end + 2:], False
        s = re.sub(r"//.*$", "", s)
        while "/*" in s:
            a = s.find("/*"); b = s.find("*/", a + 2)
            if b < 0:
                s, in_block = s[:a], True
            else:
                s = s[:a] + s[b + 2:]
        if s.strip():
            out.append((s, i))
    return out

CTRL_RE = re.compile(r"^\s*(if|for|while|switch|catch|do|else|return|case)\b")

def find_functions(text):
    """Heuristic: a signature line opening a {...} body, at any nesting.

    We do NOT gate on brace depth (MrDocs wraps everything in namespaces). We
    descend through namespace/class bodies line by line, and when we match a
    function signature we brace-match its body and skip past it, so we never
    re-detect nested lambdas as separate functions.
    """
    lines = text.splitlines()
    funcs, i, n = [], 0, len(lines)
    while i < n:
        line = lines[i]
        if (not SKIP_OPENERS.match(line) and not CTRL_RE.match(line)
                and "(" in line and SIG_RE.search(line)):
            # gather up to 4 lines to locate the opening brace
            j, blob = i, line
            while j < n and "{" not in blob and ";" not in blob and j - i < 4:
                j += 1
                if j < n:
                    blob += "\n" + lines[j]
            if j < n and "{" in blob and ";" not in blob.split("{")[0]:
                d, k, started = 0, i, False
                while k < n:
                    d += lines[k].count("{") - lines[k].count("}")
                    if "{" in lines[k]:
                        started = True
                    if started and d <= 0:
                        break
                    k += 1
                funcs.append((k - i + 1, i + 1, re.sub(r"\s+", " ", line.strip())[:80]))
                i = k + 1
                continue
        i += 1
    return funcs

def find_duplicates(files_lines, window):
    """Hash sliding windows of normalized code lines; report blocks seen >= 2x."""
    seen = {}
    for path, cl in files_lines.items():
        norm = [re.sub(r"\s+", " ", s.strip()) for s, _ in cl]
        origs = [o for _, o in cl]
        for a in range(0, len(norm) - window + 1):
            block_lines = norm[a:a + window]
            # Skip include clusters and trivial brace/keyword noise (not real dup logic).
            boilerplate = sum(1 for ln in block_lines
                              if ln.startswith("#") or ln in ("{", "}", "};", "return;"))
            if boilerplate > window * 0.4:
                continue
            block = "\n".join(block_lines)
            if len(block) < window * 8:
                continue
            h = hashlib.md5(block.encode()).hexdigest()
            seen.setdefault(h, []).append((path, origs[a] + 1))
    return {h: locs for h, locs in seen.items() if len(locs) >= 2}

def rel(p):
    return os.path.relpath(p, ROOT)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--min-func", type=int, default=80)
    ap.add_argument("--dup-window", type=int, default=12)
    ap.add_argument("--roots", nargs="+", default=["src", "include"])
    ap.add_argument("--md", default=None, help="write the worklist to this markdown file")
    args = ap.parse_args()

    files = list_files(args.roots)
    file_lengths, all_funcs, files_lines = [], [], {}
    for f in files:
        text = open(f, encoding="utf-8", errors="replace").read()
        file_lengths.append((len(text.splitlines()), rel(f)))
        files_lines[f] = code_lines(text)
        for length, ln, name in find_functions(sanitize(text)):
            if length >= args.min_func:
                all_funcs.append((length, rel(f), ln, name))
    file_lengths.sort(reverse=True)
    all_funcs.sort(reverse=True)
    dups = find_duplicates(files_lines, args.dup_window)
    dup_groups = sorted(dups.values(), key=len, reverse=True)

    lines = []
    lines.append(f"# Code metrics worklist ({len(files)} files under {', '.join(args.roots)})\n")
    lines.append(f"## Longest files (top {args.top})\n")
    for length, p in file_lengths[:args.top]:
        lines.append(f"- {length:6d}  {p}")
    lines.append(f"\n## Longest functions (heuristic, >= {args.min_func} lines, top {args.top})\n")
    for length, p, ln, name in all_funcs[:args.top]:
        lines.append(f"- {length:5d}  {p}:{ln}  `{name}`")
    lines.append(f"\n## Near-duplicate blocks ({args.dup_window}+ identical normalized lines, {len(dup_groups)} groups)\n")
    for grp in dup_groups[:args.top]:
        locs = ", ".join(f"{p}:{ln}" for p, ln in grp[:6])
        more = "" if len(grp) <= 6 else f" (+{len(grp)-6} more)"
        lines.append(f"- x{len(grp)}: {locs}{more}")
    out = "\n".join(lines) + "\n"
    print(out)
    if args.md:
        open(args.md, "w", encoding="utf-8").write(out)
        print(f"[written to {args.md}]", file=sys.stderr)

if __name__ == "__main__":
    main()
