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

import argparse
import os
import subprocess


def git(git_exe, args, source_dir):
    return subprocess.run(
        [git_exe, *args], cwd=source_dir,
        capture_output=True, text=True)


def version_with_build(git_exe, version, source_dir):
    """Return (version_with_build, full_sha) from the git state.

    Git is optional here: CMake already found it (or decided it was not required)
    before this runs, so on any git failure we just fall back to the plain version.
    """
    try:
        head = git(git_exe, ["rev-parse", "HEAD"], source_dir)
    except OSError:
        # git executable not found: fall back to the plain version.
        return version, ""
    if head.returncode != 0:
        return version, ""

    full_sha = head.stdout.strip()
    short_sha = full_sha[:12]

    # Exactly on a tag: canonical release, no build metadata.
    on_tag = git(git_exe, ["describe", "--tags", "--exact-match", "HEAD"], source_dir).returncode == 0
    if on_tag:
        return version, full_sha

    # Off a tag: append the short SHA, plus .modified if the tree is dirty.
    dirty = git(git_exe, ["diff", "--quiet", "--ignore-submodules"], source_dir).returncode == 1
    suffix = ".modified" if dirty else ""
    return f"{version}+{short_sha}{suffix}", full_sha


def main():
    parser = argparse.ArgumentParser(description="Generate Version.hpp from Version.hpp.in")
    parser.add_argument("template", help="path to Version.hpp.in")
    parser.add_argument("output", help="path to the Version.hpp to generate")
    parser.add_argument("--version", required=True, help="project version (MAJOR.MINOR.PATCH)")
    parser.add_argument("--name", default="", help="project name")
    parser.add_argument("--description", default="", help="project description")
    parser.add_argument("--source-dir", default=".", help="repository root, used for the git queries")
    parser.add_argument("--git", default="git",
                        help="git executable (CMake passes the one it found; defaults to PATH)")
    args = parser.parse_args()

    git_exe = args.git or "git"
    build_version, build_sha = version_with_build(git_exe, args.version, args.source_dir)
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
        os.makedirs(os.path.dirname(args.output), exist_ok=True)
        with open(args.output, "w") as f:
            f.write(content)


if __name__ == "__main__":
    main()
