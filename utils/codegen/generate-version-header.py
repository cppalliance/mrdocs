#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Generates Version.hpp from Version.hpp.in by substituting @PROJECT_VERSION@ and
# friends, stamping the git build metadata. Like the other codegen scripts, it is
# meant to run at build time so the SHA reflects the commit being built; it only
# rewrites the output when the content changes, so it never forces a rebuild.
#
# Two labels are stamped:
#
# - The project version (MAJOR.MINOR.PATCH from CMake), with the short SHA
#   appended when HEAD is not exactly on a tag.
# - The release label (YYYY.M.D), which is the calendar release this build
#   belongs to. On a commit carrying a calendar release tag it is that tag;
#   otherwise it is the UTC commit date of HEAD with the short SHA appended.
#   Deriving it from git rather than the wall clock keeps builds reproducible.
#   The year, month, and day are also exposed separately, mirroring the
#   MAJOR/MINOR/PATCH decomposition of the version.
#
# `--print-release` skips the header and prints the release fields instead,
# for CMake to embed them in the installed mrdocs-config.cmake.

import argparse
import os
import re
import subprocess
from datetime import datetime, timezone

# Calendar release tags: YYYY.M.D with optional semver build metadata for a
# second release on the same day (2026.9.2, 2026.9.2+1, ...).
RELEASE_TAG_RE = re.compile(r"^(20\d{2})\.(\d{1,2})\.(\d{1,2})(?:\+(\d+))?$")


def git(git_exe, args, source_dir):
    return subprocess.run(
        [git_exe, *args], cwd=source_dir,
        capture_output=True, text=True)


def release_tag_key(tag):
    m = RELEASE_TAG_RE.match(tag)
    return tuple(int(g or 0) for g in m.groups())


def git_state(git_exe, source_dir):
    """Return (full_sha, short_sha, on_tag, release_tag, commit_utc, dirty).

    Git is optional here: CMake already found it (or decided it was not required)
    before this runs, so on any git failure every field is empty/False and the
    caller falls back to the plain version.
    """
    empty = ("", "", False, "", None, False)
    try:
        head = git(git_exe, ["rev-parse", "HEAD"], source_dir)
    except OSError:
        # git executable not found: fall back to the plain version.
        return empty
    if head.returncode != 0:
        return empty

    full_sha = head.stdout.strip()
    short_sha = full_sha[:12]

    # Exactly on a tag: canonical release, no build metadata.
    on_tag = git(git_exe, ["describe", "--tags", "--exact-match", "HEAD"], source_dir).returncode == 0

    # Calendar release tags pointing at HEAD (there is normally one; if a
    # commit got re-released the same day, prefer the newest label).
    points_at = git(git_exe, ["tag", "--points-at", "HEAD"], source_dir)
    release_tags = [t for t in points_at.stdout.split() if RELEASE_TAG_RE.match(t)]
    release_tag = max(release_tags, key=release_tag_key) if release_tags else ""

    commit_utc = None
    committed = git(git_exe, ["log", "-1", "--format=%ct", "HEAD"], source_dir)
    if committed.returncode == 0 and committed.stdout.strip().isdigit():
        commit_utc = datetime.fromtimestamp(int(committed.stdout.strip()), tz=timezone.utc)

    dirty = git(git_exe, ["diff", "--quiet", "--ignore-submodules"], source_dir).returncode == 1
    return full_sha, short_sha, on_tag, release_tag, commit_utc, dirty


def version_with_build(version, state):
    """Return the project version with build metadata."""
    full_sha, short_sha, on_tag, _, _, dirty = state
    if not full_sha or on_tag:
        return version
    suffix = ".modified" if dirty else ""
    return f"{version}+{short_sha}{suffix}"


def release_labels(state):
    """Return (release, release_with_build, (year, month, day)).

    The labels are empty and the components are zero when git information
    was unavailable.
    """
    _, short_sha, _, release_tag, commit_utc, dirty = state
    if release_tag:
        year, month, day, _ = release_tag_key(release_tag)
        return release_tag, release_tag, (year, month, day)
    if commit_utc is None:
        return "", "", (0, 0, 0)
    release = f"{commit_utc.year}.{commit_utc.month}.{commit_utc.day}"
    suffix = ".modified" if dirty else ""
    return release, f"{release}+{short_sha}{suffix}", (commit_utc.year, commit_utc.month, commit_utc.day)


def main():
    parser = argparse.ArgumentParser(description="Generate Version.hpp from Version.hpp.in")
    parser.add_argument("template", nargs="?", help="path to Version.hpp.in")
    parser.add_argument("output", nargs="?", help="path to the Version.hpp to generate")
    parser.add_argument("--version", help="project version (MAJOR.MINOR.PATCH)")
    parser.add_argument("--print-release", action="store_true",
                        help="print 'release;release_with_build;year;month;day' and exit "
                             "(CMake uses this at configure time for the installed package config)")
    parser.add_argument("--name", default="", help="project name")
    parser.add_argument("--description", default="", help="project description")
    parser.add_argument("--source-dir", default=".", help="repository root, used for the git queries")
    parser.add_argument("--git", default="git",
                        help="git executable (CMake passes the one it found; defaults to PATH)")
    args = parser.parse_args()

    git_exe = args.git or "git"
    state = git_state(git_exe, args.source_dir)
    release, release_with_build, (year, month, day) = release_labels(state)
    if args.print_release:
        print(f"{release};{release_with_build};{year};{month};{day}")
        return
    if not (args.template and args.output and args.version):
        parser.error("template, output, and --version are required unless --print-release is given")

    build_version = version_with_build(args.version, state)
    build_sha = state[0]
    # Split MAJOR.MINOR.PATCH, defaulting any missing component to 0.
    parts = (args.version.split(".") + ["0", "0", "0"])[:3]
    substitutions = {
        "PROJECT_NAME": args.name,
        "PROJECT_DESCRIPTION": args.description,
        "PROJECT_VERSION": args.version,
        "PROJECT_VERSION_MAJOR": parts[0],
        "PROJECT_VERSION_MINOR": parts[1],
        "PROJECT_VERSION_PATCH": parts[2],
        "PROJECT_VERSION_WITH_BUILD": build_version,
        "PROJECT_VERSION_BUILD": build_sha,
        "PROJECT_RELEASE": release,
        "PROJECT_RELEASE_WITH_BUILD": release_with_build,
        "PROJECT_RELEASE_YEAR": str(year),
        "PROJECT_RELEASE_MONTH": str(month),
        "PROJECT_RELEASE_DAY": str(day),
    }

    content = ''
    with open(args.template, "r") as f:
        content = f.read()
    for name, value in substitutions.items():
        content = content.replace(f"@{name}@", value)

    # Only write when the content changed, so an unchanged SHA never touches the
    # file (and never triggers a rebuild of everything that includes it).
    previous = None
    if os.path.exists(args.output):
        with open(args.output, "r") as f:
            previous = f.read()
    if content != previous:
        if os.path.dirname(args.output):
            os.makedirs(os.path.dirname(args.output), exist_ok=True)
        with open(args.output, "w") as f:
            f.write(content)


if __name__ == "__main__":
    main()
