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

"""
Git utility functions for the bootstrap tool.

Provides functions for handling Git repositories, including symlink
repair for Windows systems where core.symlinks=false.
"""

import os
import shutil
import subprocess
from typing import List, Tuple, Optional

from .filesystem import ensure_dir, write_text
from .ui import TextUI, get_default_ui


def is_git_repo(repo_dir: str, git_path: str = "git") -> bool:
    """
    Check if a directory is a Git work tree.

    Args:
        repo_dir: The directory to check.
        git_path: Path to the git executable.

    Returns:
        True if repo_dir is inside a Git work tree.
    """
    if os.path.isdir(os.path.join(repo_dir, ".git")):
        return True
    try:
        out = subprocess.check_output(
            [git_path, "-C", repo_dir, "rev-parse", "--is-inside-work-tree"],
            stderr=subprocess.DEVNULL, text=True
        )
        return out.strip() == "true"
    except Exception:
        return False


def git_symlink_entries(repo_dir: str, git_path: str = "git") -> List[Tuple[str, str]]:
    """
    Get all git-tracked symlinks (mode 120000) in a repository.

    Args:
        repo_dir: The repository directory.
        git_path: Path to the git executable.

    Returns:
        List of (worktree_path, intended_target_string) tuples.
    """
    out = subprocess.check_output(
        [git_path, "-C", repo_dir, "ls-files", "-s"],
        text=True, encoding="utf-8", errors="replace"
    )
    entries = []
    for line in out.splitlines():
        # Format: "<mode> <object> <stage>\t<path>"
        # Symlinks have mode 120000
        try:
            head, path = line.split("\t", 1)
            mode, obj, _stage = head.split()[:3]
        except ValueError:
            continue
        if mode != "120000":
            continue
        target = subprocess.check_output(
            [git_path, "-C", repo_dir, "cat-file", "-p", obj],
            text=True, encoding="utf-8", errors="replace"
        ).rstrip("\n")
        entries.append((path, target))
    return entries


def same_link_target(link_path: str, intended: str) -> bool:
    """
    Check if a symlink points to the intended target (normalized).

    Args:
        link_path: Path to the symlink.
        intended: The intended target path.

    Returns:
        True if link_path points to intended.
    """
    try:
        current = os.readlink(link_path)
    except OSError:
        return False

    def norm(p):
        return os.path.normpath(p.replace("/", os.sep))

    return norm(current) == norm(intended)


def make_symlink_or_fallback(
    file_path: str,
    intended_target: str,
    repo_dir: str,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
) -> str:
    """
    Create a symlink at file_path pointing to intended_target.

    Falls back to hardlink/copy on Windows if symlinks aren't permitted.

    Args:
        file_path: Path where the symlink should be created.
        intended_target: The target the symlink should point to (POSIX path from git).
        repo_dir: The repository directory (for resolving relative paths).
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.

    Returns:
        'symlink', 'hardlink', 'copy', or 'dry-run'
    """
    if ui is None:
        ui = get_default_ui()

    if dry_run:
        ui.info(f"dry-run: would ensure symlink {file_path} -> {intended_target}")
        return "dry-run"

    parent = os.path.dirname(file_path)
    if parent and not os.path.isdir(parent):
        ensure_dir(parent, dry_run=False, ui=ui)

    # Remove existing non-symlink file
    if os.path.exists(file_path) and not os.path.islink(file_path):
        os.remove(file_path)

    # Git stores POSIX-style link text; translate to native separators
    native_target = intended_target.replace("/", os.sep)

    # Detect if the final target is a directory (Windows needs this)
    resolved_target = os.path.normpath(os.path.join(parent, native_target))
    target_is_dir = os.path.isdir(resolved_target)

    # Try real symlink first
    try:
        if os.name == "nt":
            os.symlink(native_target, file_path, target_is_directory=target_is_dir)
        else:
            os.symlink(native_target, file_path)
        return "symlink"
    except (NotImplementedError, OSError, PermissionError):
        pass

    # Fallback: hardlink (files only, same volume)
    try:
        if os.path.isfile(resolved_target):
            os.link(resolved_target, file_path)
            return "hardlink"
    except OSError:
        pass

    # Last resort: copy the file contents if it exists
    if os.path.isfile(resolved_target):
        shutil.copyfile(resolved_target, file_path)
        return "copy"

    # If the target doesn't exist, write the intended link text
    write_text(file_path, intended_target, encoding="utf-8", dry_run=False, ui=ui)
    return "copy"


def check_git_symlinks(
    repo_dir: str,
    git_path: str = "git",
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Ensure all Git-tracked symlinks in repo_dir are correct in the working tree.

    Fixes text-file placeholders produced when core.symlinks=false.
    This is particularly important on Windows.

    Args:
        repo_dir: The repository directory to check.
        git_path: Path to the git executable.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    repo_dir = os.path.abspath(repo_dir)
    if not is_git_repo(repo_dir, git_path):
        return

    symlinks = git_symlink_entries(repo_dir, git_path)
    if not symlinks:
        return

    fixed = {"symlink": 0, "hardlink": 0, "copy": 0, "already_ok": 0}

    for rel_path, intended in symlinks:
        link_path = os.path.join(repo_dir, rel_path)

        # Already OK?
        if os.path.islink(link_path) and same_link_target(link_path, intended):
            fixed["already_ok"] += 1
            continue

        # If it's a regular file, replace it
        kind = make_symlink_or_fallback(link_path, intended, repo_dir, dry_run=dry_run, ui=ui)
        fixed[kind] += 1

    # Summary + Windows hint
    total_fixed = fixed["symlink"] + fixed["hardlink"] + fixed["copy"]
    if total_fixed > 0:
        ui.info(
            f"Repaired Git symlinks in {ui.shorten_path(repo_dir)} "
            f"(created: {fixed['symlink']} symlink(s), {fixed['hardlink']} hardlink(s), "
            f"{fixed['copy']} copy/copies; {fixed['already_ok']} already OK)."
        )
        if fixed["hardlink"] or fixed["copy"]:
            ui.warn(
                "Some symlinks could not be created. On Windows, enable Developer Mode "
                "or run with privileges that allow creating symlinks. Also ensure "
                "`git config core.symlinks true` before checkout."
            )
