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
Recipe source fetching utilities.

Provides functions to download and fetch recipe source code from
git repositories and archives.
"""

import json
import os
import shlex
import shutil
import urllib.request
from typing import Optional

from ..core.filesystem import ensure_dir, remove_dir, write_text
from ..core.process import run_cmd
from ..core.ui import TextUI, get_default_ui
from .schema import Recipe
from .archive import extract_zip_flatten, extract_tar_flatten


def build_archive_url(url: str, ref: str) -> Optional[str]:
    """
    Build a GitHub archive download URL for a commit or tag.

    Args:
        url: Repository URL.
        ref: Git reference (commit, tag, or branch).

    Returns:
        Archive URL, or None if not a GitHub URL.
    """
    if "github.com" not in url or not ref:
        return None
    # Strip .git and trailing slash
    clean = url
    if clean.endswith(".git"):
        clean = clean[:-4]
    clean = clean.rstrip("/")
    parts = clean.split("github.com/", 1)[1].split("/")
    if len(parts) < 2:
        return None
    owner, repo = parts[0], parts[1]
    return f"https://github.com/{owner}/{repo}/archive/{ref}.zip"


def recipe_stamp_path(recipe: Recipe) -> str:
    """Get the path to the recipe's stamp file."""
    return os.path.join(recipe.install_dir, ".bootstrap-stamp.json")


def is_recipe_up_to_date(
    recipe: Recipe,
    resolved_ref: str,
    sanitizer: str = "",
    cc: str = "",
    cxx: str = "",
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
) -> bool:
    """
    Check if a recipe is already built and up to date.

    Args:
        recipe: The recipe to check.
        resolved_ref: The resolved git reference.
        sanitizer: Sanitizer used for the build.
        cc: C compiler path.
        cxx: C++ compiler path.
        cflags: Extra C flags.
        cxxflags: Extra C++ flags.
        ldflags: Extra linker flags.

    Returns:
        True if the recipe is up to date.
    """
    stamp_path = recipe_stamp_path(recipe)
    if not os.path.exists(stamp_path):
        return False
    try:
        with open(stamp_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception:
        return False
    if data.get("version") != recipe.version or data.get("ref") != resolved_ref:
        return False
    # If the stamp has a content hash, verify it matches the current
    # recipe and runtime parameters. Stamps without a hash (from older
    # bootstrap versions) pass this check.
    stored_hash = data.get("content_hash")
    if stored_hash is not None:
        current_hash = _recipe_content_hash(
            recipe, sanitizer, cc, cxx, cflags, cxxflags, ldflags
        )
        if stored_hash != current_hash:
            return False
    return True


def _recipe_content_hash(
    recipe: Recipe,
    sanitizer: str = "",
    cc: str = "",
    cxx: str = "",
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
) -> str:
    """
    Compute a hash of everything that affects the build output.

    Covers both the recipe definition and runtime parameters like
    sanitizer, compiler, and flags. Any change invalidates the stamp.
    """
    import hashlib
    content = json.dumps({
        "version": recipe.version,
        "source_url": recipe.source.url,
        "source_ref": recipe.source.commit or recipe.source.tag or recipe.source.branch or recipe.source.ref or "",
        "build": recipe.build,
        "build_type": recipe.build_type,
        "tags": recipe.tags,
        "sanitizer": sanitizer,
        "cc": cc,
        "cxx": cxx,
        "cflags": cflags,
        "cxxflags": cxxflags,
        "ldflags": ldflags,
    }, sort_keys=True)
    return hashlib.sha256(content.encode()).hexdigest()[:16]


def write_recipe_stamp(
    recipe: Recipe,
    resolved_ref: str,
    sanitizer: str = "",
    cc: str = "",
    cxx: str = "",
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Write a stamp file to mark a recipe as built.

    Args:
        recipe: The recipe that was built.
        resolved_ref: The resolved git reference.
        sanitizer: Sanitizer used for the build.
        cc: C compiler path.
        cxx: C++ compiler path.
        cflags: Extra C flags.
        cxxflags: Extra C++ flags.
        ldflags: Extra linker flags.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    stamp = recipe_stamp_path(recipe)
    payload = {
        "name": recipe.name,
        "version": recipe.version,
        "ref": resolved_ref,
        "content_hash": _recipe_content_hash(
            recipe, sanitizer, cc, cxx, cflags, cxxflags, ldflags
        ),
    }
    ensure_dir(recipe.install_dir, dry_run=dry_run, ui=ui)
    write_text(stamp, json.dumps(payload, indent=2), dry_run=dry_run, ui=ui)


def download_file(
    url: str,
    dest: str,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Download a file from a URL.

    Args:
        url: URL to download from.
        dest: Destination file path.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    if dry_run:
        print(f"curl -L -o {shlex.quote(dest)} {shlex.quote(url)}")
        return

    parent = os.path.dirname(dest)
    if parent:
        ensure_dir(parent, dry_run=False, ui=ui)

    print(f"Downloading {url}...")
    urllib.request.urlretrieve(url, dest)


def fetch_recipe_source(
    recipe: Recipe,
    source_dir: str,
    git_path: str = "git",
    clean: bool = False,
    force: bool = False,
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[dict] = None,
    ui: Optional[TextUI] = None,
) -> str:
    """
    Fetch the source code for a recipe.

    Args:
        recipe: The recipe to fetch.
        source_dir: MrDocs source directory.
        git_path: Path to git executable.
        clean: If True, remove existing source and re-download.
        force: If True, force re-download even if up to date.
        dry_run: If True, only print what would be done.
        verbose: If True, show verbose output.
        debug: If True, show debug output.
        env: Environment variables for commands.
        ui: TextUI instance for output.

    Returns:
        The resolved git reference.
    """
    if ui is None:
        ui = get_default_ui()

    src = recipe.source
    dest = recipe.source_dir
    resolved_ref = src.commit or src.tag or src.branch or src.ref or ""

    if clean and os.path.exists(dest):
        remove_dir(dest, dry_run=dry_run, ui=ui)

    if not dry_run and not force and is_recipe_up_to_date(recipe, resolved_ref):
        ui.ok(f"[{recipe.name}] already up to date ({resolved_ref or 'HEAD'}).")
        return resolved_ref

    # If source already exists and we're not forcing or cleaning, skip re-download
    # In dry-run mode, always show the fetch commands for a complete manual reference.
    if not dry_run and os.path.isdir(dest) and not clean and not force:
        ui.info(f"{recipe.name}: source already present at {ui.shorten_path(dest)}; skipping download.")
        return resolved_ref or "HEAD"

    # Try to build archive URL
    archive_url = None
    if src.type == "git":
        archive_url = build_archive_url(src.url, src.commit or src.tag or src.ref)
    elif src.type in ("archive", "http", "zip"):
        archive_url = src.url

    if archive_url:
        filename = os.path.basename(archive_url.split("?")[0])
        tmp_archive = os.path.join(source_dir, "build", "third-party", "source", filename)
        download_file(archive_url, tmp_archive, dry_run=dry_run, ui=ui)

        if dry_run or os.path.exists(dest):
            remove_dir(dest, dry_run=dry_run, ui=ui)
        ensure_dir(dest, dry_run=dry_run, ui=ui)

        if archive_url.endswith(".zip"):
            extract_zip_flatten(tmp_archive, dest, dry_run=dry_run, ui=ui)
        else:
            extract_tar_flatten(tmp_archive, dest, dry_run=dry_run, ui=ui)
        if not dry_run:
            os.remove(tmp_archive)
        else:
            print(f"rm {shlex.quote(tmp_archive)}")
    else:
        # Fallback to git clone
        depth_args = ["--depth", str(src.depth)] if src.depth else []
        if not os.path.exists(dest):
            ensure_dir(os.path.dirname(dest), dry_run=dry_run, ui=ui)
            clone_cmd = [git_path, "-c", "core.symlinks=true", "clone", src.url, dest, *depth_args]
            if src.branch and not src.commit:
                clone_cmd.extend(["--branch", src.branch])
            run_cmd(clone_cmd, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

        if resolved_ref:
            run_cmd([git_path, "fetch", "--tags"], cwd=dest, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)
            run_cmd([git_path, "checkout", resolved_ref], cwd=dest, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)
        else:
            run_cmd([git_path, "pull", "--ff-only"], cwd=dest, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

    return resolved_ref or "HEAD"


def apply_recipe_patches(
    recipe: Recipe,
    patches_dir: str,
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[dict] = None,
    ui: Optional[TextUI] = None,
):
    """
    Apply patches to a recipe's source.

    Args:
        recipe: The recipe to patch.
        patches_dir: Root directory containing patch directories.
        dry_run: If True, only print what would be done.
        verbose: If True, show verbose output.
        debug: If True, show debug output.
        env: Environment variables for commands.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    patch_root = os.path.join(patches_dir, recipe.name)
    if not os.path.isdir(patch_root):
        return

    entries = sorted(os.listdir(patch_root))
    for entry in entries:
        path = os.path.join(patch_root, entry)
        if entry.endswith(".patch"):
            ui.info(f"Applying patch {path}")
            run_cmd(
                ["patch", "-p1", "-i", path],
                cwd=recipe.source_dir,
                dry_run=dry_run,
                verbose=verbose,
                debug=debug,
                env=env,
                ui=ui,
            )
        else:
            target = os.path.join(recipe.source_dir, entry)
            if os.path.isdir(path):
                if dry_run:
                    print(f"cp -r {shlex.quote(path)} {shlex.quote(target)}")
                else:
                    shutil.copytree(path, target, dirs_exist_ok=True)
            else:
                if dry_run:
                    print(f"cp {shlex.quote(path)} {shlex.quote(target)}")
                else:
                    ensure_dir(os.path.dirname(target), dry_run=False, ui=ui)
                    shutil.copy(path, target)
