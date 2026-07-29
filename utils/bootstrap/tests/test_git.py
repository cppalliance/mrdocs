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

"""Tests for core/git.py functions."""

import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock, call

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

from src.core.git import (
    is_git_repo,
    git_symlink_entries,
    same_link_target,
    make_symlink_or_fallback,
    check_git_symlinks,
)
from src.core.ui import TextUI


def _make_ui():
    """Create a plain TextUI for testing (no color, no emoji)."""
    return TextUI()


# ── is_git_repo ──────────────────────────────────────────────────────

class TestIsGitRepo(unittest.TestCase):

    def test_returns_true_when_dot_git_dir_exists(self):
        """Returns True if .git directory exists, without calling subprocess."""
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, ".git"))
            self.assertTrue(is_git_repo(tmp))

    def test_returns_true_via_subprocess(self):
        """Returns True when subprocess reports inside work tree."""
        with tempfile.TemporaryDirectory() as tmp:
            with patch("src.core.git.subprocess.check_output", return_value="true\n"):
                self.assertTrue(is_git_repo(tmp))

    def test_returns_false_via_subprocess(self):
        """Returns False when subprocess reports not a work tree."""
        with tempfile.TemporaryDirectory() as tmp:
            with patch("src.core.git.subprocess.check_output", return_value="false\n"):
                self.assertFalse(is_git_repo(tmp))

    def test_returns_false_on_exception(self):
        """Returns False when subprocess raises."""
        with tempfile.TemporaryDirectory() as tmp:
            with patch("src.core.git.subprocess.check_output",
                        side_effect=subprocess.CalledProcessError(1, "git")):
                self.assertFalse(is_git_repo(tmp))

    def test_custom_git_path(self):
        """Passes custom git_path to subprocess."""
        with tempfile.TemporaryDirectory() as tmp:
            with patch("src.core.git.subprocess.check_output", return_value="true\n") as mock_co:
                is_git_repo(tmp, git_path="/usr/local/bin/git")
                args = mock_co.call_args[0][0]
                self.assertEqual(args[0], "/usr/local/bin/git")


# ── git_symlink_entries ──────────────────────────────────────────────

class TestGitSymlinkEntries(unittest.TestCase):

    def test_parses_symlink_entries(self):
        """Parses mode-120000 entries and fetches their targets."""
        ls_output = (
            "100644 abc123 0\tREADME.md\n"
            "120000 def456 0\tsome/link\n"
            "100644 ghi789 0\tother.txt\n"
        )
        with patch("src.core.git.subprocess.check_output") as mock_co:
            mock_co.side_effect = [
                ls_output,        # ls-files -s
                "target/path\n",  # cat-file -p for def456
            ]
            result = git_symlink_entries("/repo")

        self.assertEqual(result, [("some/link", "target/path")])

    def test_multiple_symlinks(self):
        """Returns all symlink entries."""
        ls_output = (
            "120000 aaa 0\tlink1\n"
            "120000 bbb 0\tlink2\n"
        )
        with patch("src.core.git.subprocess.check_output") as mock_co:
            mock_co.side_effect = [
                ls_output,
                "target1\n",
                "target2\n",
            ]
            result = git_symlink_entries("/repo")

        self.assertEqual(len(result), 2)
        self.assertEqual(result[0], ("link1", "target1"))
        self.assertEqual(result[1], ("link2", "target2"))

    def test_no_symlinks(self):
        """Returns empty list when no mode-120000 entries."""
        ls_output = "100644 abc123 0\tREADME.md\n"
        with patch("src.core.git.subprocess.check_output", return_value=ls_output):
            result = git_symlink_entries("/repo")
        self.assertEqual(result, [])

    def test_malformed_lines_skipped(self):
        """Skips lines that can't be parsed."""
        ls_output = "garbage line without tab\n120000 aaa 0\tgood\n"
        with patch("src.core.git.subprocess.check_output") as mock_co:
            mock_co.side_effect = [ls_output, "tgt\n"]
            result = git_symlink_entries("/repo")
        self.assertEqual(result, [("good", "tgt")])


# ── same_link_target ─────────────────────────────────────────────────

class TestSameLinkTarget(unittest.TestCase):

    def test_matching_target(self):
        """Returns True when readlink matches intended."""
        with patch("src.core.git.os.readlink", return_value="foo/bar"):
            self.assertTrue(same_link_target("/some/link", "foo/bar"))

    def test_non_matching_target(self):
        """Returns False when readlink differs."""
        with patch("src.core.git.os.readlink", return_value="other/path"):
            self.assertFalse(same_link_target("/some/link", "foo/bar"))

    def test_oserror_returns_false(self):
        """Returns False when readlink raises OSError."""
        with patch("src.core.git.os.readlink", side_effect=OSError("not a link")):
            self.assertFalse(same_link_target("/some/link", "foo/bar"))

    def test_normalizes_slashes(self):
        """Normalizes path separators for comparison."""
        with patch("src.core.git.os.readlink", return_value="foo/bar"):
            self.assertTrue(same_link_target("/link", "foo/bar"))

    def test_normalizes_dot_components(self):
        """Normalizes ../ components in paths."""
        with patch("src.core.git.os.readlink", return_value="a/../b"):
            self.assertTrue(same_link_target("/link", "b"))


# ── make_symlink_or_fallback ─────────────────────────────────────────

class TestMakeSymlinkOrFallback(unittest.TestCase):

    def test_dry_run_prints_and_returns(self):
        """Dry-run mode prints ln command and returns 'dry-run'."""
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            result = make_symlink_or_fallback(
                "/repo/link", "target", "/repo", dry_run=True, ui=_make_ui()
            )
        self.assertEqual(result, "dry-run")
        self.assertIn("ln -sf", mock_out.getvalue())

    def test_creates_symlink_on_unix(self):
        """Creates symlink and returns 'symlink' on success."""
        with tempfile.TemporaryDirectory() as tmp:
            target_file = os.path.join(tmp, "target.txt")
            with open(target_file, "w") as f:
                f.write("content")
            link_path = os.path.join(tmp, "mylink")
            result = make_symlink_or_fallback(
                link_path, "target.txt", tmp, ui=_make_ui()
            )
            self.assertEqual(result, "symlink")
            self.assertTrue(os.path.islink(link_path))
            self.assertEqual(os.readlink(link_path), "target.txt")

    def test_creates_parent_dirs(self):
        """Creates parent directories if they don't exist."""
        with tempfile.TemporaryDirectory() as tmp:
            target_file = os.path.join(tmp, "target.txt")
            with open(target_file, "w") as f:
                f.write("content")
            link_path = os.path.join(tmp, "sub", "dir", "mylink")
            result = make_symlink_or_fallback(
                link_path, "../../target.txt", tmp, ui=_make_ui()
            )
            self.assertEqual(result, "symlink")
            self.assertTrue(os.path.islink(link_path))

    def test_removes_existing_non_symlink(self):
        """Removes existing regular file before creating symlink."""
        with tempfile.TemporaryDirectory() as tmp:
            target_file = os.path.join(tmp, "target.txt")
            with open(target_file, "w") as f:
                f.write("real")
            link_path = os.path.join(tmp, "mylink")
            with open(link_path, "w") as f:
                f.write("placeholder")
            result = make_symlink_or_fallback(
                link_path, "target.txt", tmp, ui=_make_ui()
            )
            self.assertEqual(result, "symlink")
            self.assertTrue(os.path.islink(link_path))

    def test_fallback_to_copy_when_symlink_fails(self):
        """Falls back to copy when symlink raises OSError."""
        with tempfile.TemporaryDirectory() as tmp:
            target_file = os.path.join(tmp, "target.txt")
            with open(target_file, "w") as f:
                f.write("content")
            link_path = os.path.join(tmp, "mylink")
            with patch("src.core.git.os.symlink", side_effect=OSError("no symlink")):
                with patch("src.core.git.os.link", side_effect=OSError("no hardlink")):
                    result = make_symlink_or_fallback(
                        link_path, "target.txt", tmp, ui=_make_ui()
                    )
            self.assertEqual(result, "copy")
            with open(link_path) as f:
                self.assertEqual(f.read(), "content")

    def test_fallback_to_hardlink_when_symlink_fails(self):
        """Falls back to hardlink when symlink raises OSError."""
        with tempfile.TemporaryDirectory() as tmp:
            target_file = os.path.join(tmp, "target.txt")
            with open(target_file, "w") as f:
                f.write("content")
            link_path = os.path.join(tmp, "mylink")
            with patch("src.core.git.os.symlink", side_effect=OSError("no symlink")):
                result = make_symlink_or_fallback(
                    link_path, "target.txt", tmp, ui=_make_ui()
                )
            self.assertEqual(result, "hardlink")

    def test_writes_link_text_when_target_missing(self):
        """Writes intended target text when resolved target doesn't exist."""
        with tempfile.TemporaryDirectory() as tmp:
            link_path = os.path.join(tmp, "mylink")
            with patch("src.core.git.os.symlink", side_effect=OSError("no symlink")):
                result = make_symlink_or_fallback(
                    link_path, "nonexistent/target", tmp, ui=_make_ui()
                )
            self.assertEqual(result, "copy")
            with open(link_path) as f:
                self.assertEqual(f.read(), "nonexistent/target")

    @patch("src.core.git.os.name", "nt")
    def test_windows_passes_target_is_directory(self):
        """On Windows (os.name == 'nt'), passes target_is_directory to os.symlink."""
        with tempfile.TemporaryDirectory() as tmp:
            link_path = os.path.join(tmp, "mylink")
            with patch("src.core.git.os.symlink") as mock_sym:
                make_symlink_or_fallback(
                    link_path, "target", tmp, ui=_make_ui()
                )
                mock_sym.assert_called_once()
                _, kwargs = mock_sym.call_args
                self.assertIn("target_is_directory", kwargs)


# ── check_git_symlinks ───────────────────────────────────────────────

class TestCheckGitSymlinks(unittest.TestCase):

    def test_dry_run_prints_message(self):
        """Dry-run prints a comment about repairing symlinks."""
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            check_git_symlinks("/repo", dry_run=True, ui=_make_ui())
        output = mock_out.getvalue()
        self.assertIn("dry-run", output.lower())

    def test_not_a_git_repo_returns_early(self):
        """Returns early without processing if not a git repo."""
        with patch("src.core.git.is_git_repo", return_value=False) as mock_is:
            with patch("src.core.git.git_symlink_entries") as mock_entries:
                check_git_symlinks("/not-a-repo", ui=_make_ui())
                mock_is.assert_called_once()
                mock_entries.assert_not_called()

    def test_no_symlinks_returns_early(self):
        """Returns early when no symlink entries found."""
        with patch("src.core.git.is_git_repo", return_value=True):
            with patch("src.core.git.git_symlink_entries", return_value=[]):
                with patch("src.core.git.make_symlink_or_fallback") as mock_make:
                    check_git_symlinks("/repo", ui=_make_ui())
                    mock_make.assert_not_called()

    def test_already_ok_symlinks_not_fixed(self):
        """Symlinks that are already correct are counted but not recreated."""
        ui = _make_ui()
        with patch("src.core.git.is_git_repo", return_value=True):
            with patch("src.core.git.git_symlink_entries",
                        return_value=[("link1", "target1")]):
                with patch("src.core.git.os.path.islink", return_value=True):
                    with patch("src.core.git.same_link_target", return_value=True):
                        with patch("src.core.git.make_symlink_or_fallback") as mock_make:
                            check_git_symlinks("/repo", ui=ui)
                            mock_make.assert_not_called()

    def test_broken_symlinks_get_fixed(self):
        """Broken symlinks are fixed via make_symlink_or_fallback."""
        ui = _make_ui()
        with patch("src.core.git.is_git_repo", return_value=True):
            with patch("src.core.git.git_symlink_entries",
                        return_value=[("link1", "target1"), ("link2", "target2")]):
                with patch("src.core.git.os.path.islink", return_value=False):
                    with patch("src.core.git.make_symlink_or_fallback",
                                return_value="symlink") as mock_make:
                        check_git_symlinks("/repo", ui=ui)
                        self.assertEqual(mock_make.call_count, 2)

    def test_mixed_ok_and_broken(self):
        """Mix of already-OK and broken symlinks."""
        ui = _make_ui()
        with patch("src.core.git.is_git_repo", return_value=True):
            with patch("src.core.git.git_symlink_entries",
                        return_value=[("ok_link", "tgt1"), ("broken_link", "tgt2")]):
                # ok_link is a symlink with correct target; broken_link is not a symlink
                def islink_side(path):
                    return "ok_link" in path

                def same_target_side(path, intended):
                    return "ok_link" in path

                with patch("src.core.git.os.path.islink", side_effect=islink_side):
                    with patch("src.core.git.same_link_target", side_effect=same_target_side):
                        with patch("src.core.git.make_symlink_or_fallback",
                                    return_value="symlink") as mock_make:
                            check_git_symlinks("/repo", ui=ui)
                            self.assertEqual(mock_make.call_count, 1)

    def test_info_and_warn_on_hardlink_fallback(self):
        """Prints info summary and Windows warning when hardlinks used."""
        ui = _make_ui()
        with patch("src.core.git.is_git_repo", return_value=True):
            with patch("src.core.git.git_symlink_entries",
                        return_value=[("link1", "tgt1")]):
                with patch("src.core.git.os.path.islink", return_value=False):
                    with patch("src.core.git.make_symlink_or_fallback",
                                return_value="hardlink"):
                        with patch.object(ui, "info") as mock_info:
                            with patch.object(ui, "warn") as mock_warn:
                                check_git_symlinks("/repo", ui=ui)
                                mock_info.assert_called_once()
                                mock_warn.assert_called_once()
                                self.assertIn("symlink", mock_info.call_args[0][0].lower())

    def test_no_warn_when_only_symlinks(self):
        """No Windows warning when all fixes are real symlinks."""
        ui = _make_ui()
        with patch("src.core.git.is_git_repo", return_value=True):
            with patch("src.core.git.git_symlink_entries",
                        return_value=[("link1", "tgt1")]):
                with patch("src.core.git.os.path.islink", return_value=False):
                    with patch("src.core.git.make_symlink_or_fallback",
                                return_value="symlink"):
                        with patch.object(ui, "warn") as mock_warn:
                            check_git_symlinks("/repo", ui=ui)
                            mock_warn.assert_not_called()


if __name__ == "__main__":
    unittest.main()
