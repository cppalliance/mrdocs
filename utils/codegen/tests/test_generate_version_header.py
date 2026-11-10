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

"""Tests for utils/codegen/generate-version-header.py, run on temporary git repositories."""

import importlib.util
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "generate_version_header", os.path.join(os.path.dirname(HERE), "generate-version-header.py"))
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)


def git(repo, *args):
    subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", *args],
                   cwd=str(repo), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def git_version():
    out = subprocess.run(["git", "--version"], check=True, capture_output=True, text=True).stdout
    found = re.search(r"(\d+)\.(\d+)", out)
    return (int(found.group(1)), int(found.group(2))) if found else (0, 0)


# The describe placeholder in git-archival.txt is expanded by the git that
# builds the archive, and `%(describe)` in a log format needs git 2.32.
needs_format_describe = unittest.skipIf(
    git_version() < (2, 32), "git before 2.32 does not expand %(describe) in --format")


class BuildIdTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.repo = self.tmp / "repo"
        self.repo.mkdir()
        git(self.repo, "init", "-q")
        (self.repo / "a.txt").write_text("a\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "base")

    def tag(self, name="2026.11.2"):
        git(self.repo, "tag", name)

    def with_build(self):
        state = gen.git_state("git", str(self.repo))
        return gen.release_labels(state)[1], state[1]

    def build_id(self):
        return gen.plugin_build_id(gen.git_state("git", str(self.repo)))

    def test_plugin_id_is_the_commit_tagged_or_not(self):
        sha = gen.git_state("git", str(self.repo))[1]
        self.assertRegex(sha, r"^[0-9a-f]{12}$")
        self.assertEqual(self.build_id(), sha)
        self.tag()
        self.assertEqual(self.build_id(), sha)

    def test_plugin_id_modified(self):
        self.tag()
        (self.repo / "a.txt").write_text("b\n")
        sha = gen.git_state("git", str(self.repo))[1]
        self.assertEqual(self.build_id(), f"{sha}.modified")

    def test_plugin_id_from_archive_is_the_node(self):
        archive = self.archive_file("2026.9.29")
        with mock.patch.dict(os.environ, {"GIT_CEILING_DIRECTORIES": str(self.tmp)}):
            state = gen.git_state("git", str(self.tmp / "nowhere"), archive=str(archive))
        self.assertEqual(gen.plugin_build_id(state), "0123456789ab")

    def test_plugin_id_without_git_is_empty(self):
        with mock.patch.dict(os.environ, {"GIT_CEILING_DIRECTORIES": str(self.tmp)}):
            state = gen.git_state("git", str(self.tmp / "nowhere"), archive=str(self.tmp / "none.txt"))
        self.assertEqual(gen.plugin_build_id(state), "")

    def test_untagged_clean(self):
        build, sha = self.with_build()
        self.assertTrue(build.endswith(f"+{sha}"))

    def test_untagged_modified(self):
        (self.repo / "a.txt").write_text("b\n")
        build, sha = self.with_build()
        self.assertTrue(build.endswith(f"+{sha}.modified"))

    def test_tagged_clean_is_bare_tag(self):
        self.tag()
        self.assertEqual(self.with_build()[0], "2026.11.2")

    def test_tagged_unstaged_edit(self):
        self.tag()
        (self.repo / "a.txt").write_text("b\n")
        build, sha = self.with_build()
        self.assertEqual(build, f"2026.11.2+{sha}.modified")

    def test_tagged_staged_edit(self):
        self.tag()
        (self.repo / "a.txt").write_text("b\n")
        git(self.repo, "add", "a.txt")
        build, sha = self.with_build()
        self.assertEqual(build, f"2026.11.2+{sha}.modified")

    def test_tagged_untracked_file(self):
        self.tag()
        (self.repo / "new.txt").write_text("n\n")
        build, sha = self.with_build()
        self.assertEqual(build, f"2026.11.2+{sha}.modified")

    def test_ignored_file_is_clean(self):
        (self.repo / ".gitignore").write_text("*.log\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "ignore")
        self.tag()
        (self.repo / "x.log").write_text("n\n")
        self.assertEqual(self.with_build()[0], "2026.11.2")

    def test_no_git_gives_empty_id(self):
        with mock.patch.dict(os.environ, {"GIT_CEILING_DIRECTORIES": str(self.tmp)}):
            state = gen.git_state("git", str(self.tmp / "nowhere"), archive=str(self.tmp / "none.txt"))
        self.assertEqual(gen.release_labels(state)[1], "")

    def test_tagged_version_with_build_is_modified(self):
        self.tag()
        (self.repo / "a.txt").write_text("b\n")
        state = gen.git_state("git", str(self.repo))
        self.assertEqual(gen.version_with_build("1.2.3", state), f"1.2.3+{state[1]}.modified")

    def archive_file(self, describe):
        archive = self.tmp / "git-archival.txt"
        archive.write_text(
            "node: 0123456789abcdef0123456789abcdef01234567\n"
            "node-date: 1793000000\n"
            f"describe-name: {describe}\n")
        return archive

    def archive_labels(self, archive):
        with mock.patch.dict(os.environ, {"GIT_CEILING_DIRECTORIES": str(self.tmp)}):
            state = gen.git_state("git", str(self.tmp / "nowhere"), archive=str(archive))
        return gen.release_labels(state), gen.version_with_build("0.8.0", state)

    def test_archive_of_a_release_tag(self):
        labels, version = self.archive_labels(self.archive_file("2026.9.29"))
        self.assertEqual(labels, ("2026.9.29", "2026.9.29", (2026, 9, 29)))
        self.assertEqual(version, "0.8.0")

    def test_archive_of_a_second_release_tag_of_the_day(self):
        labels, version = self.archive_labels(self.archive_file("2026.9.29+1"))
        self.assertEqual(labels, ("2026.9.29+1", "2026.9.29+1", (2026, 9, 29)))
        self.assertEqual(version, "0.8.0")

    def test_archive_past_a_release_tag_keeps_the_tag(self):
        labels, version = self.archive_labels(self.archive_file("2026.9.29-89-gfd6da6b4eea"))
        self.assertEqual(labels, ("2026.9.29", "2026.9.29+0123456789ab", (2026, 9, 29)))
        self.assertEqual(version, "0.8.0+0123456789ab")

    def test_archive_past_a_second_release_of_the_day(self):
        labels, _ = self.archive_labels(self.archive_file("2026.9.29+1-3-gfd6da6b4eea"))
        self.assertEqual(labels, ("2026.9.29+1", "2026.9.29+1.0123456789ab", (2026, 9, 29)))

    def test_archive_with_a_describe_that_is_no_release_uses_date(self):
        labels, version = self.archive_labels(self.archive_file("v1-87-g907daf428ce"))
        self.assertEqual(labels, ("2026.10.26", "2026.10.26+0123456789ab", (2026, 10, 26)))
        self.assertEqual(version, "0.8.0+0123456789ab")

    def test_past_a_release_tag_keeps_the_tag(self):
        self.tag("2026.9.29")
        (self.repo / "b.txt").write_text("b\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "next")
        state = gen.git_state("git", str(self.repo))
        sha = state[1]
        self.assertEqual(len(sha), 12)
        self.assertEqual(gen.release_labels(state), ("2026.9.29", f"2026.9.29+{sha}", (2026, 9, 29)))
        self.assertEqual(gen.version_with_build("0.8.0", state), f"0.8.0+{sha}")

    def test_past_a_release_tag_and_modified(self):
        self.tag("2026.9.29")
        (self.repo / "b.txt").write_text("b\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "next")
        (self.repo / "b.txt").write_text("c\n")
        state = gen.git_state("git", str(self.repo))
        self.assertEqual(gen.release_labels(state)[1], f"2026.9.29+{state[1]}.modified")

    def test_past_two_release_tags_uses_the_newest(self):
        self.tag("2026.9.2")
        (self.repo / "b.txt").write_text("b\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "next")
        self.tag("2026.9.29")
        (self.repo / "c.txt").write_text("c\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "later")
        self.assertEqual(gen.release_labels(gen.git_state("git", str(self.repo)))[0], "2026.9.29")

    def test_past_a_non_release_tag_uses_date(self):
        self.tag("2027-draft")
        (self.repo / "b.txt").write_text("b\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "next")
        state = gen.git_state("git", str(self.repo))
        release, build, _ = gen.release_labels(state)
        self.assertEqual(release, f"{state[4].year}.{state[4].month}.{state[4].day}")
        self.assertTrue(build.endswith(f"+{state[1]}"))

    def commit(self, name):
        (self.repo / name).write_text(name)
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", name)

    def test_non_calendar_tags_do_not_hide_the_release(self):
        self.tag("2026.9.29")
        self.commit("b.txt")
        self.tag("2026.9.29-rc1")
        self.tag("2027-draft")
        state = gen.git_state("git", str(self.repo))
        self.assertEqual(gen.release_labels(state), ("2026.9.29", f"2026.9.29+{state[1]}", (2026, 9, 29)))

    def test_release_tag_on_head_beside_a_non_calendar_tag(self):
        self.tag("2026.9.29")
        self.tag("2026.9.29-rc1")
        self.assertEqual(gen.release_labels(gen.git_state("git", str(self.repo)))[1], "2026.9.29")

    @needs_format_describe
    def test_archival_placeholder_skips_non_calendar_tags(self):
        archival = Path(gen.ARCHIVE_FILE).read_text()
        fmt = [l for l in archival.splitlines() if l.startswith("describe-name:")][0]
        fmt = fmt.split("$Format:", 1)[1].rsplit("$", 1)[0]
        self.tag("2026.9.29")
        self.commit("b.txt")
        self.tag("2026.9.29-rc1")
        self.tag("2027-draft")
        out = subprocess.run(["git", "log", "-1", f"--format={fmt}", "HEAD"], cwd=str(self.repo),
                             check=True, capture_output=True, text=True).stdout.strip()
        self.assertRegex(out, r"^2026\.9\.29-1-g[0-9a-f]+$")

    @needs_format_describe
    def test_git_and_archive_agree_across_a_merge(self):
        self.tag("2026.9.29")
        git(self.repo, "checkout", "-q", "-b", "side")
        self.commit("side.txt")
        self.tag("2026.10.1")
        git(self.repo, "checkout", "-q", "-")
        self.commit("main.txt")
        git(self.repo, "merge", "-q", "--no-ff", "-m", "merge", "side")
        fmt = [l for l in Path(gen.ARCHIVE_FILE).read_text().splitlines() if l.startswith("describe-name:")][0]
        fmt = fmt.split("$Format:", 1)[1].rsplit("$", 1)[0]
        describe = subprocess.run(["git", "log", "-1", f"--format={fmt}", "HEAD"], cwd=str(self.repo),
                                  check=True, capture_output=True, text=True).stdout.strip()
        sha = subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(self.repo),
                             check=True, capture_output=True, text=True).stdout.strip()
        archive = self.tmp / "git-archival.txt"
        archive.write_text(f"node: {sha}\nnode-date: 1793000000\ndescribe-name: {describe}\n")
        from_git = gen.git_state("git", str(self.repo))
        from_archive = gen.archive_state(str(archive))
        self.assertEqual(gen.release_labels(from_git)[:2], gen.release_labels(from_archive)[:2])

    def test_print_release_past_a_tag(self):
        self.tag("2026.9.29")
        (self.repo / "b.txt").write_text("b\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "next")
        script = os.path.join(os.path.dirname(HERE), "generate-version-header.py")
        out = subprocess.run(["python3", "-I", script, "--print-release", "--version", "0.8.0",
                              "--source-dir", str(self.repo)], check=True, capture_output=True, text=True).stdout
        fields = out.strip().split(";")
        self.assertEqual(fields[0], "2026.9.29")
        self.assertRegex(fields[1], r"^2026\.9\.29\+[0-9a-f]{12}$")
        self.assertEqual(fields[2:5], ["2026", "9", "29"])
        self.assertRegex(fields[5], r"^[0-9a-f]{12}$")

    def test_archive_without_describe_uses_date_and_hash(self):
        archive = self.tmp / "git-archival.txt"
        archive.write_text("node: 0123456789abcdef0123456789abcdef01234567\nnode-date: 1793000000\n")
        self.assertEqual(self.archive_labels(archive)[0][1], "2026.10.26+0123456789ab")

    def test_unexpanded_archive_placeholder_is_no_id(self):
        archive = self.tmp / "git-archival.txt"
        archive.write_text(
            "node: $Format:%H$\nnode-date: $Format:%ct$\n"
            "describe-name: $Format:%(describe:tags=true,match=20*)$\n")
        self.assertIsNone(gen.archive_state(str(archive)))

    def test_unexpanded_archive_uses_git(self):
        archive = self.tmp / "git-archival.txt"
        archive.write_text("node: $Format:%H$\nnode-date: $Format:%ct$\n")
        self.tag()
        state = gen.git_state("git", str(self.repo), archive=str(archive))
        self.assertEqual(gen.release_labels(state)[0], "2026.11.2")

    def test_space_separated_line_is_not_an_archive(self):
        archive = self.tmp / "git-archival.txt"
        archive.write_text("0123456789abcdef0123456789abcdef01234567 1793000000\n")
        self.assertIsNone(gen.archive_state(str(archive)))

    def test_build_id_override_replaces_the_plugin_id_only(self):
        self.tag("2026.9.29")
        out = self.tmp / "Version.hpp"
        template = self.tmp / "Version.hpp.in"
        template.write_text("@PROJECT_RELEASE_WITH_BUILD@ @PROJECT_BUILD_ID@\n")
        script = os.path.join(os.path.dirname(HERE), "generate-version-header.py")
        subprocess.run(["python3", "-I", script, str(template), str(out), "--version", "1.2.3",
                        "--source-dir", str(self.repo), "--build-id", "port-7"], check=True)
        self.assertEqual(out.read_text().strip(), "2026.9.29 port-7")
        printed = subprocess.run(["python3", "-I", script, "--print-release", "--source-dir", str(self.repo),
                                  "--build-id", "port-7"], check=True, capture_output=True, text=True).stdout
        self.assertEqual(printed.strip(), "2026.9.29;2026.9.29;2026;9;29;port-7")


if __name__ == "__main__":
    unittest.main()
