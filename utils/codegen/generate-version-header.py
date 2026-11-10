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
# friends. It runs at build time, like the other codegen scripts, and only
# rewrites the output when the content changes, so it never forces a rebuild.
#
# The script prints three labels:
#
# - The project version (MAJOR.MINOR.PATCH from CMake), with the short SHA
#   appended when HEAD is not exactly on a tag. It is what the project calls
#   itself, such as `0.8.0` on a release and `0.8.0+fd6da6b4eeaf` 89 commits
#   later.
# - The release label (YYYY.M.D), the calendar release this build belongs to.
#   It is the nearest calendar release tag, so a build of
#   89 commits after the tag 2026.9.29 is still release 2026.9.29. When the
#   commit is not on that tag, the short SHA is appended: `2026.9.29` on the
#   tag, `2026.9.29+fd6da6b4eeaf` 89 commits later. Users can compare that
#   to a real release, which a commit date would not let them do. After a
#   second release of the day (`2026.9.29+1`) the SHA joins with a dot:
#   `2026.9.29+1.fd6da6b4eeaf`.
#
#   When no calendar tag is reachable, the label is the UTC commit date of
#   HEAD with the short SHA appended. That happens in a clone made without the
#   tags (shallow, or `--no-tags`), which therefore gets a different id from
#   a tarball of the same commit.
#
#   When the tree differs from HEAD or has untracked files, `.modified` is
#   added after the SHA: a tag with edits is `2026.9.29+6c6169a96328.modified`.
#
#   The year, month, and day are also exposed separately, mirroring the
#   MAJOR/MINOR/PATCH decomposition of the version. They come from the same
#   tag, or from the commit date when there is no tag.
#
#   Off a release, only the label with the SHA tells such a build apart from
#   the release itself.
# - The plugin build id, the id plugins are checked against. It is the commit
#   alone, the first 12 digits of the SHA, so the same code has the same id
#   whether or not it was tagged when it was built: `fd6da6b4eeaf` on the tag
#   and 89 commits later alike. When the tree differs from HEAD or has
#   untracked files, `.modified` follows: `fd6da6b4eeaf.modified`.
#
# `--print-release` skips the header and prints the release fields instead,
# for CMake to embed them in the installed mrdocs-config.cmake. It prints six
# fields separated by semicolons. For an edited tree 89 commits after the tag
# 2026.9.29 it prints:
#
#   2026.9.29;2026.9.29+fd6da6b4eeaf.modified;2026;9;29;fd6da6b4eeaf.modified
#
# These are the fields, with the variable CMake sets for each:
#
#   1. release             2026.9.29                         MRDOCS_RELEASE
#   2. release with build  2026.9.29+fd6da6b4eeaf.modified   MRDOCS_RELEASE_WITH_BUILD
#   3. year                2026                              MRDOCS_RELEASE_YEAR
#   4. month               9                                 MRDOCS_RELEASE_MONTH
#   5. day                 29                                MRDOCS_RELEASE_DAY
#   6. plugin build id     fd6da6b4eeaf.modified             MRDOCS_BUILD_ID_VALUE
#
# On the tag 2026.9.29 itself, with no edits, fields 1 and 2 are both
# 2026.9.29, and field 6 is still the commit: 6a984f66cf77.
#
# The data is derived from the commit information. `git-archival.txt` is how
# a source archive carries it. The file is checked in with placeholders:
#
#   node: $Format:%H$
#   node-date: $Format:%ct$
#   describe-name: $Format:%(describe:tags=true,match=20*,exclude=*[!0-9.+]*)$
#
# `git archive` fills them in. In an unpacked GitHub tarball of the tag
# 2026.9.29 it reads:
#
#   node: 6a984f66cf770ee5808bc0f53614c522c68fa507
#   node-date: 1790725028
#   describe-name: 2026.9.29
#
# and in a tarball of HEAD 89 commits later:
#
#   node: fd6da6b4eeaf0c3a91d5e7b2486f1a09c3d7e5b4
#   node-date: 1793000000
#   describe-name: 2026.9.29-89-gfd6da6b4eea
#
# `describe-name` is what `git describe` reports about the commit: the
# nearest calendar tag (the `exclude` skips tags such as `2026.9.29-rc1`),
# and when the commit is past it, the number of commits since and the start
# of the SHA. Both tarballs give release 2026.9.29. The second gives the
# release label `2026.9.29+fd6da6b4eeaf` (the first 12 digits of `node`) and
# the plugin id `fd6da6b4eeaf`, the same as a checkout of that commit. When
# `describe-name` is empty, the script uses the commit date of `node-date`
# instead.
#
# The script takes its data from these sources, tried in this order:
#
# 1. `--build-id` (CMake's MRDOCS_BUILD_ID) sets field 6 only, the plugin id,
#    whichever way the commit was found. It is the packager's identity for the
#    plugin check, and it leaves the release label alone so the label stays
#    honest. With `--build-id debian1`, the example above becomes:
#
#      2026.9.29;2026.9.29+fd6da6b4eeaf.modified;2026;9;29;debian1
#
# 2. `git-archival.txt`, when `git archive` filled it in. It sets fields 1 to
#    6, and it wins even when git is installed.
# 3. git, when the file still holds its `$Format:` placeholders, as in a
#    checkout. It sets fields 1 to 6: the tag, the commit date, the commit,
#    and whether the tree is modified. The tag is the nearest calendar one, from
#    the same `git describe` as the placeholder above.

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


DESCRIBE_SUFFIX_RE = re.compile(r"-\d+-g[0-9a-f]+$")


def parse_describe(describe):
    """Return (release_tag, exact) for the output of `git describe`.

    `exact` is True when the commit is the tag itself. Both are empty/False
    when the name is not based on a calendar release tag.
    """
    if RELEASE_TAG_RE.match(describe):
        return describe, True
    base = DESCRIBE_SUFFIX_RE.sub("", describe)
    if base != describe and RELEASE_TAG_RE.match(base):
        return base, False
    return "", False


def release_tag_key(tag):
    m = RELEASE_TAG_RE.match(tag)
    return tuple(int(g or 0) for g in m.groups())


ARCHIVE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "git-archival.txt")


def archive_state(path=ARCHIVE_FILE):
    """Return the state recorded in a `git archive` export, or None.

    `.gitattributes` marks `git-archival.txt` with `export-subst`, so an
    archive holds one `key: value` line per field:

        node: <sha>
        node-date: <unix commit time>
        describe-name: <tag, or tag-N-g<sha>>

    A checkout holds the unexpanded `$Format:` placeholders instead. The
    calendar tag in `describe-name` is the nearest release the commit has;
    a name that is not based on one is ignored.
    """
    try:
        with open(path, "r", encoding="utf-8") as f:
            lines = f.read().splitlines()
    except OSError:
        return None
    fields = {}
    for line in lines:
        key, sep, value = line.partition(":")
        if sep:
            fields[key.strip()] = value.strip()
    node = fields.get("node", "")
    date = fields.get("node-date", "")
    if not re.fullmatch(r"[0-9a-f]{40,64}", node) or not date.isdigit():
        return None
    describe = fields.get("describe-name", "")
    release_tag, exact = parse_describe(describe)
    commit_utc = datetime.fromtimestamp(int(date), tz=timezone.utc)
    return node, node[:12], exact, release_tag, commit_utc, False, exact


def git_state(git_exe, source_dir, archive=ARCHIVE_FILE):
    """Return (full_sha, short_sha, on_tag, release_tag, commit_utc, dirty, on_release).

    `release_tag` is the nearest calendar release tag,
    and `on_release` tells whether the commit is that tag.

    Git is optional here: CMake already found it (or decided it was not required)
    before this runs, so on any git failure every field is empty/False and the
    caller falls back to the plain version.
    """
    empty = ("", "", False, "", None, False, False)
    # An expanded archive file means this is an export, whatever repository
    # it was unpacked into.
    exported = archive_state(archive)
    if exported:
        return exported
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

    # The nearest calendar release tag, chosen by the same describe call as
    # the placeholder in git-archival.txt, so a checkout and an archive of one
    # commit agree. The exclude skips tags such as 2026.9.29-rc1. A commit
    # that carries two release tags takes the newest.
    described = git(git_exe, ["describe", "--tags", "--match", "20*", "--exclude", "*[!0-9.+]*",
                              "--abbrev=12", "HEAD"], source_dir)
    release_tag, on_release = parse_describe(described.stdout.strip()) if described.returncode == 0 else ("", False)
    if on_release:
        points_at = git(git_exe, ["tag", "--points-at", "HEAD"], source_dir)
        release_tags = [t for t in points_at.stdout.split() if RELEASE_TAG_RE.match(t)]
        release_tag = max(release_tags, key=release_tag_key)

    commit_utc = None
    committed = git(git_exe, ["log", "-1", "--format=%ct", "HEAD"], source_dir)
    if committed.returncode == 0 and committed.stdout.strip().isdigit():
        commit_utc = datetime.fromtimestamp(int(committed.stdout.strip()), tz=timezone.utc)

    # Modified means any difference from HEAD, staged or not, or any file
    # git does not track and does not ignore.
    differs = git(git_exe, ["diff", "--quiet", "HEAD", "--ignore-submodules"], source_dir).returncode == 1
    untracked = git(git_exe, ["ls-files", "--others", "--exclude-standard"], source_dir).stdout.strip() != ""
    dirty = differs or untracked
    return full_sha, short_sha, on_tag, release_tag, commit_utc, dirty, on_release


def version_with_build(version, state):
    """Return the project version with build metadata."""
    full_sha, short_sha, on_tag, _, _, dirty, _ = state
    if not full_sha:
        return version
    if on_tag and not dirty:
        return version
    suffix = ".modified" if dirty else ""
    return f"{version}+{short_sha}{suffix}"


def release_labels(state):
    """Return (release, release_with_build, (year, month, day)).

    The labels are empty and the components are zero when git information
    was unavailable.
    """
    _, short_sha, _, release_tag, commit_utc, dirty, on_release = state
    if release_tag:
        year, month, day, _ = release_tag_key(release_tag)
        if on_release and not dirty:
            return release_tag, release_tag, (year, month, day)
        suffix = ".modified" if dirty else ""
        joiner = "." if "+" in release_tag else "+"
        return release_tag, f"{release_tag}{joiner}{short_sha}{suffix}", (year, month, day)
    if commit_utc is None:
        return "", "", (0, 0, 0)
    release = f"{commit_utc.year}.{commit_utc.month}.{commit_utc.day}"
    suffix = ".modified" if dirty else ""
    return release, f"{release}+{short_sha}{suffix}", (commit_utc.year, commit_utc.month, commit_utc.day)


def plugin_build_id(state):
    """Return the commit the plugins are checked against, or "" without one.

    It is the short SHA, with `.modified` when the tree has edits, so the id
    does not depend on whether the commit was tagged.
    """
    _, short_sha, _, _, _, dirty, _ = state
    if not short_sha:
        return ""
    return f"{short_sha}.modified" if dirty else short_sha


def main():
    parser = argparse.ArgumentParser(description="Generate Version.hpp from Version.hpp.in")
    parser.add_argument("template", nargs="?", help="path to Version.hpp.in")
    parser.add_argument("output", nargs="?", help="path to the Version.hpp to generate")
    parser.add_argument("--version", help="project version (MAJOR.MINOR.PATCH)")
    parser.add_argument("--print-release", action="store_true",
                        help="print 'release;release_with_build;year;month;day;build_id' and exit "
                             "(CMake uses this at configure time for the installed package config)")
    parser.add_argument("--name", default="", help="project name")
    parser.add_argument("--description", default="", help="project description")
    parser.add_argument("--source-dir", default=".", help="repository root, used for the git queries")
    parser.add_argument("--build-id", default="",
                        help="plugin build id that replaces the commit derived from git or the "
                             "archive (CMake passes MRDOCS_BUILD_ID)")
    parser.add_argument("--git", default="git",
                        help="git executable (CMake passes the one it found; defaults to PATH)")
    args = parser.parse_args()

    git_exe = args.git or "git"
    state = git_state(git_exe, args.source_dir)
    release, release_with_build, (year, month, day) = release_labels(state)
    build_id = args.build_id or plugin_build_id(state)
    if args.print_release:
        print(f"{release};{release_with_build};{year};{month};{day};{build_id}")
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
        "PROJECT_BUILD_ID": build_id,
    }

    content = ''
    with open(args.template, "r", encoding="utf-8") as f:
        content = f.read()
    for name, value in substitutions.items():
        content = content.replace(f"@{name}@", value)

    # Only write when the content changed, so an unchanged SHA never touches the
    # file (and never triggers a rebuild of everything that includes it).
    previous = None
    if os.path.exists(args.output):
        with open(args.output, "r", encoding="utf-8") as f:
            previous = f.read()
    if content != previous:
        if os.path.dirname(args.output):
            os.makedirs(os.path.dirname(args.output), exist_ok=True)
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(content)


if __name__ == "__main__":
    main()
