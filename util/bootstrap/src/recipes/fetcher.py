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
) -> str:
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
        Empty string if the recipe is up to date, otherwise a reason
        explaining why the stamp doesn't match.
    """
    stamp_path = recipe_stamp_path(recipe)
    if not os.path.exists(stamp_path):
        return "no stamp file found"
    try:
        with open(stamp_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception:
        return "stamp file is corrupt or unreadable"
    if data.get("ref") != resolved_ref:
        return f"ref changed: {data.get('ref')!r} -> {resolved_ref!r}"
    # Check if the recipe definition changed.
    # New format stores individual fields ("recipe" dict); old format
    # stores an opaque hash ("recipe_hash" string).  Both are accepted.
    stored_recipe = data.get("recipe")
    if stored_recipe is not None:
        current_recipe = _recipe_fields(recipe)
        for key in set(list(stored_recipe.keys()) + list(current_recipe.keys())):
            old_val = stored_recipe.get(key)
            new_val = current_recipe.get(key)
            if old_val != new_val:
                diff = _field_diff(old_val, new_val)
                return f"recipe {key} changed: {diff}"
    elif data.get("recipe_hash") is not None:
        # Old format with opaque hash — verify it still matches
        current_hash = _recipe_hash(recipe)
        if data["recipe_hash"] != current_hash:
            return f"recipe changed (hash {data['recipe_hash']!r} -> {current_hash!r})"
    # Check if the platform changed (OS, version, architecture).
    stored_platform = data.get("platform")
    if stored_platform is not None:
        current_platform = _platform_info()
        for key in set(list(stored_platform.keys()) + list(current_platform.keys())):
            old_val = stored_platform.get(key)
            new_val = current_platform.get(key)
            if old_val != new_val:
                return f"platform {key} changed: {old_val!r} -> {new_val!r}"
    # Check runtime build parameters (sanitizer, compiler, flags).
    # Stamps from older versions may have "content_hash" or
    # "build_params" with recipe fields mixed in -- treat as stale.
    stored_params = data.get("build_params")
    if stored_params is None:
        # Old-format stamps (with content_hash instead of build_params)
        # can't be compared field-by-field. Treat as stale so the stamp
        # gets rewritten in the new format.
        has_keys = [k for k in data if k not in ("name", "version", "ref")]
        return f"stamp uses old format (has: {has_keys})"
    current_params = _build_params(sanitizer, cc, cxx, cflags, cxxflags, ldflags)
    for key in set(list(stored_params.keys()) + list(current_params.keys())):
        old_val = stored_params.get(key)
        new_val = current_params.get(key)
        if old_val != new_val:
            return f"{key} changed: {_field_diff(old_val, new_val)}"
    return ""


def _field_diff(old_val, new_val) -> str:
    """Produce a human-readable diff between two field values.

    For JSON-encoded strings, parse them and show only the parts
    that differ.  For simple values, show old -> new.
    """
    if old_val is None:
        return f"added: {new_val!r}"
    if new_val is None:
        return f"removed: {old_val!r}"
    # Try parsing as JSON for a deeper diff
    try:
        old_parsed = json.loads(old_val) if isinstance(old_val, str) else old_val
        new_parsed = json.loads(new_val) if isinstance(new_val, str) else new_val
        return _value_diff(old_parsed, new_parsed)
    except (json.JSONDecodeError, TypeError):
        pass
    return f"{old_val!r} -> {new_val!r}"


def _value_diff(old, new, indent=2) -> str:
    """Recursively diff two parsed values, showing only what changed."""
    pad = " " * indent
    if old == new:
        return f"{pad}(unchanged)"
    if isinstance(old, dict) and isinstance(new, dict):
        diffs = []
        for k in sorted(set(list(old.keys()) + list(new.keys()))):
            ov = old.get(k)
            nv = new.get(k)
            if ov == nv:
                continue
            if ov is None:
                diffs.append(f"{pad}{k}: added {nv!r}")
            elif nv is None:
                diffs.append(f"{pad}{k}: removed {ov!r}")
            elif isinstance(ov, (dict, list)) and isinstance(nv, (dict, list)):
                diffs.append(f"{pad}{k}:")
                diffs.append(_value_diff(ov, nv, indent + 2))
            else:
                diffs.append(f"{pad}{k}: {ov!r} -> {nv!r}")
        return "\n".join(diffs)
    if isinstance(old, list) and isinstance(new, list):
        if len(old) == len(new):
            # Same-length lists: diff element by element
            diffs = []
            for i, (ov, nv) in enumerate(zip(old, new)):
                if ov != nv:
                    diffs.append(f"{pad}[{i}]:")
                    diffs.append(_value_diff(ov, nv, indent + 2))
            return "\n".join(diffs) if diffs else f"{pad}(unchanged)"
        # Different-length lists: show added/removed
        only_old = [x for x in old if x not in new]
        only_new = [x for x in new if x not in old]
        parts = []
        if only_old:
            parts.append(f"{pad}removed: {only_old!r}")
        if only_new:
            parts.append(f"{pad}added: {only_new!r}")
        return "\n".join(parts) if parts else f"{pad}{old!r} -> {new!r}"
    return f"{pad}{old!r} -> {new!r}"


def _recipe_fields(recipe: Recipe) -> dict:
    """
    Collect the recipe definition fields that affect the build.

    Covers every field from the recipe file.  Computed fields
    (source_dir, build_dir, install_dir) are excluded since they
    depend on the environment.  Values are JSON-serialized so
    complex types (lists, dicts) can be compared as strings.
    """
    import dataclasses
    return {
        "name": recipe.name,
        "version": recipe.version,
        "source": json.dumps(dataclasses.asdict(recipe.source), sort_keys=True),
        "dependencies": json.dumps(recipe.dependencies, sort_keys=True),
        "build": json.dumps(recipe.build, sort_keys=True),
        "build_type": recipe.build_type,
        "tags": json.dumps(recipe.tags, sort_keys=True) if recipe.tags else "[]",
        "install_scope": recipe.install_scope,
        "package_root_var": recipe.package_root_var,
    }


def _recipe_hash(recipe: Recipe) -> str:
    """Compute a hash of the recipe fields for backward compatibility
    with old-format stamps that stored 'recipe_hash'."""
    import hashlib
    content = json.dumps(_recipe_fields(recipe), sort_keys=True)
    return hashlib.sha256(content.encode()).hexdigest()[:16]


def _platform_info() -> dict:
    """
    Collect platform information that affects binary compatibility.

    A change in OS, OS version, or architecture means cached binaries
    may not work.
    """
    import platform
    info = {
        "os": platform.system(),
        "arch": platform.machine(),
    }
    # Get OS version where available
    if platform.system() == "Linux":
        try:
            import distro
            info["os_version"] = distro.version()
        except ImportError:
            # Fall back to reading /etc/os-release directly
            try:
                with open("/etc/os-release") as f:
                    for line in f:
                        if line.startswith("VERSION_ID="):
                            info["os_version"] = line.split("=", 1)[1].strip().strip('"')
                            break
            except OSError:
                pass
    elif platform.system() == "Darwin":
        info["os_version"] = platform.mac_ver()[0]
    elif platform.system() == "Windows":
        info["os_version"] = platform.version()
    return info


def _build_params(
    sanitizer: str = "",
    cc: str = "",
    cxx: str = "",
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
) -> dict:
    """
    Collect runtime build parameters that affect the build output.

    Only includes non-empty values.  These are compared field by field
    so the mismatch reason identifies exactly what changed.
    """
    params = {}
    if sanitizer:
        params["sanitizer"] = sanitizer
    if cc:
        params["cc"] = cc
    if cxx:
        params["cxx"] = cxx
    if cflags:
        params["cflags"] = cflags
    if cxxflags:
        params["cxxflags"] = cxxflags
    if ldflags:
        params["ldflags"] = ldflags
    return params


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
        "recipe": _recipe_fields(recipe),
        "platform": _platform_info(),
        "build_params": _build_params(sanitizer, cc, cxx, cflags, cxxflags, ldflags),
    }
    ensure_dir(recipe.install_dir, dry_run=dry_run, ui=ui)
    content = json.dumps(payload, indent=2)
    write_text(stamp, content, dry_run=dry_run, ui=ui)
    if not dry_run:
        ui.info(f"Stamp written to {stamp}")
        ui.info(f"Stamp contents:\n{content}")


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

    if not dry_run and not force and not is_recipe_up_to_date(recipe, resolved_ref):
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
