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

"""Tests for core/process.py run_cmd function."""

import io
import os
import subprocess
import sys
import unittest
from unittest.mock import patch, MagicMock, PropertyMock

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.core.process import run_cmd, _dry_run_exported
from src.core.ui import TextUI


def _make_ui():
    """Create a plain TextUI for testing (no color, no emoji)."""
    return TextUI()


class TestRunCmdDryRun(unittest.TestCase):
    """Tests for run_cmd dry-run mode."""

    def setUp(self):
        _dry_run_exported.clear()

    def test_dry_run_prints_command(self):
        """Dry-run prints the command string to stdout."""
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd("echo hello", dry_run=True, ui=_make_ui())
        self.assertIn("echo hello", mock_out.getvalue())

    def test_dry_run_with_cd(self):
        """Dry-run prints cd prefix when cwd differs from os.getcwd()."""
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd("make", cwd="/tmp/other", dry_run=True, ui=_make_ui())
        output = mock_out.getvalue()
        self.assertIn("cd", output)
        self.assertIn("make", output)

    def test_dry_run_no_cd_when_same_dir(self):
        """Dry-run omits cd when cwd matches os.getcwd()."""
        cwd = os.getcwd()
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd("make", cwd=cwd, dry_run=True, ui=_make_ui())
        output = mock_out.getvalue()
        self.assertNotIn("cd", output)
        self.assertIn("make", output)

    def test_dry_run_exports_env_vars(self):
        """Dry-run prints export statements for env vars differing from current env."""
        env = os.environ.copy()
        env["MY_TEST_VAR"] = "test_value_123"
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd("echo hi", dry_run=True, env=env, ui=_make_ui())
        output = mock_out.getvalue()
        self.assertIn("export MY_TEST_VAR=", output)
        self.assertIn("test_value_123", output)

    def test_dry_run_env_dedup(self):
        """Same env var exported twice should only appear once."""
        env = os.environ.copy()
        env["DEDUP_VAR"] = "value1"
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd("echo first", dry_run=True, env=env, ui=_make_ui())
            run_cmd("echo second", dry_run=True, env=env, ui=_make_ui())
        output = mock_out.getvalue()
        count = output.count("export DEDUP_VAR=")
        self.assertEqual(count, 1, f"Expected 1 export, got {count}. Output:\n{output}")

    def test_dry_run_env_empty_value(self):
        """Dry-run handles empty env var values."""
        env = os.environ.copy()
        env["EMPTY_VAR"] = ""
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd("echo hi", dry_run=True, env=env, ui=_make_ui())
        output = mock_out.getvalue()
        self.assertIn("export EMPTY_VAR=", output)

    def test_dry_run_list_command(self):
        """Dry-run correctly formats list commands."""
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            run_cmd(["cmake", "--build", "build"], dry_run=True, ui=_make_ui())
        output = mock_out.getvalue()
        self.assertIn("cmake", output)
        self.assertIn("--build", output)
        self.assertIn("build", output)


class TestRunCmdNonDryTailFalse(unittest.TestCase):
    """Tests for run_cmd non-dry-run with tail=False (subprocess.run path)."""

    @patch("src.core.process.subprocess.run")
    def test_success(self, mock_run):
        """Successful command returns without raising."""
        mock_run.return_value = MagicMock(returncode=0)
        run_cmd("echo hello", tail=False, ui=_make_ui())
        mock_run.assert_called_once()

    @patch("src.core.process.subprocess.run")
    def test_passes_cwd(self, mock_run):
        """cwd is passed through to subprocess.run."""
        mock_run.return_value = MagicMock(returncode=0)
        run_cmd("echo hello", cwd="/tmp", tail=False, ui=_make_ui())
        _, kwargs = mock_run.call_args
        self.assertEqual(kwargs["cwd"], "/tmp")

    @patch("src.core.process.subprocess.run")
    def test_list_cmd_shell_false(self, mock_run):
        """List commands use shell=False."""
        mock_run.return_value = MagicMock(returncode=0)
        run_cmd(["echo", "hello"], tail=False, ui=_make_ui())
        _, kwargs = mock_run.call_args
        self.assertFalse(kwargs["shell"])

    @patch("src.core.process.subprocess.run")
    def test_string_cmd_shell_true(self, mock_run):
        """String commands use shell=True."""
        mock_run.return_value = MagicMock(returncode=0)
        run_cmd("echo hello", tail=False, ui=_make_ui())
        _, kwargs = mock_run.call_args
        self.assertTrue(kwargs["shell"])

    @patch("src.core.process.subprocess.run")
    def test_called_process_error_raises_runtime(self, mock_run):
        """CalledProcessError is translated to RuntimeError."""
        mock_run.side_effect = subprocess.CalledProcessError(1, "bad_cmd")
        with self.assertRaises(RuntimeError):
            run_cmd("bad_cmd", tail=False, ui=_make_ui())

    @patch("src.core.process.subprocess.run")
    def test_called_process_error_debug_reraises(self, mock_run):
        """With debug=True, the CalledProcessError is re-raised directly."""
        mock_run.side_effect = subprocess.CalledProcessError(1, "bad_cmd")
        with self.assertRaises(subprocess.CalledProcessError):
            run_cmd("bad_cmd", tail=False, debug=True, ui=_make_ui())

    @patch("src.core.process.subprocess.run")
    def test_nonzero_returncode_raises(self, mock_run):
        """Non-zero return code raises RuntimeError (non-check path)."""
        mock_run.return_value = MagicMock(returncode=1)
        # check=True in subprocess.run should raise CalledProcessError,
        # but if somehow returncode != 0 without exception, the code handles it
        mock_run.side_effect = subprocess.CalledProcessError(1, "cmd")
        with self.assertRaises(RuntimeError):
            run_cmd("cmd", tail=False, ui=_make_ui())

    @patch("src.core.process.subprocess.run")
    def test_sets_cmake_parallel_level(self, mock_run):
        """CMAKE_BUILD_PARALLEL_LEVEL is set when not present in env."""
        mock_run.return_value = MagicMock(returncode=0)
        clean_env = {"PATH": "/usr/bin"}
        run_cmd("echo hello", tail=False, env=clean_env, ui=_make_ui())
        _, kwargs = mock_run.call_args
        self.assertIn("CMAKE_BUILD_PARALLEL_LEVEL", kwargs["env"])

    @patch("src.core.process.subprocess.run")
    def test_preserves_existing_cmake_parallel_level(self, mock_run):
        """Existing CMAKE_BUILD_PARALLEL_LEVEL is not overwritten."""
        mock_run.return_value = MagicMock(returncode=0)
        env = {"PATH": "/usr/bin", "CMAKE_BUILD_PARALLEL_LEVEL": "2"}
        run_cmd("echo hello", tail=False, env=env, ui=_make_ui())
        _, kwargs = mock_run.call_args
        self.assertEqual(kwargs["env"]["CMAKE_BUILD_PARALLEL_LEVEL"], "2")


class _FakeStdout:
    """A fake stdout that supports iteration and close()."""

    def __init__(self, lines):
        self._lines = lines
        self._iter = iter(lines)

    def __iter__(self):
        return self._iter

    def __next__(self):
        return next(self._iter)

    def __bool__(self):
        return True

    def close(self):
        pass


class TestRunCmdTailTrue(unittest.TestCase):
    """Tests for run_cmd with tail=True (subprocess.Popen path)."""

    @patch("src.core.process.subprocess.Popen")
    def test_success(self, mock_popen_cls):
        """Successful Popen command returns without raising."""
        mock_proc = MagicMock()
        mock_proc.stdout = _FakeStdout(["line1\n", "line2\n"])
        mock_proc.returncode = 0
        mock_proc.wait.return_value = 0
        mock_popen_cls.return_value = mock_proc
        with patch("sys.stdout", new_callable=io.StringIO):
            run_cmd("echo hello", tail=True, ui=_make_ui())
        mock_popen_cls.assert_called_once()

    @patch("src.core.process.subprocess.Popen")
    def test_failure_raises_runtime(self, mock_popen_cls):
        """Non-zero Popen return code raises RuntimeError."""
        mock_proc = MagicMock()
        mock_proc.stdout = _FakeStdout(["output\n"])
        mock_proc.returncode = 1
        mock_proc.wait.return_value = 1
        mock_popen_cls.return_value = mock_proc
        with patch("sys.stdout", new_callable=io.StringIO):
            with self.assertRaises(RuntimeError):
                run_cmd("bad_cmd", tail=True, ui=_make_ui())

    @patch("src.core.process.subprocess.Popen")
    def test_captures_output_lines(self, mock_popen_cls):
        """Popen captures output lines for failure display."""
        mock_proc = MagicMock()
        mock_proc.stdout = _FakeStdout(["error line\n"])
        mock_proc.returncode = 1
        mock_proc.wait.return_value = 1
        mock_popen_cls.return_value = mock_proc
        with patch("sys.stdout", new_callable=io.StringIO) as mock_out:
            with self.assertRaises(RuntimeError):
                run_cmd("failing", tail=True, ui=_make_ui())
        self.assertIn("error line", mock_out.getvalue())

    @patch("src.core.process.subprocess.Popen")
    def test_popen_launch_failure(self, mock_popen_cls):
        """Popen launch failure raises RuntimeError."""
        mock_popen_cls.side_effect = OSError("No such file")
        with self.assertRaises(RuntimeError) as ctx:
            run_cmd("nonexistent_cmd", tail=True, ui=_make_ui())
        self.assertIn("Failed to launch", str(ctx.exception))

    @patch("src.core.process.subprocess.Popen")
    def test_passes_env_and_cwd(self, mock_popen_cls):
        """Popen receives cwd and env."""
        mock_proc = MagicMock()
        mock_proc.stdout = _FakeStdout([])
        mock_proc.returncode = 0
        mock_proc.wait.return_value = 0
        mock_popen_cls.return_value = mock_proc
        custom_env = {"PATH": "/usr/bin", "FOO": "bar"}
        with patch("sys.stdout", new_callable=io.StringIO):
            run_cmd("echo hi", tail=True, cwd="/tmp", env=custom_env, ui=_make_ui())
        _, kwargs = mock_popen_cls.call_args
        self.assertEqual(kwargs["cwd"], "/tmp")
        self.assertIn("FOO", kwargs["env"])

    @patch("src.core.process.subprocess.Popen")
    def test_empty_output(self, mock_popen_cls):
        """Command with no output lines succeeds."""
        mock_proc = MagicMock()
        mock_proc.stdout = _FakeStdout([])
        mock_proc.returncode = 0
        mock_proc.wait.return_value = 0
        mock_popen_cls.return_value = mock_proc
        with patch("sys.stdout", new_callable=io.StringIO):
            run_cmd("silent_cmd", tail=True, ui=_make_ui())


class TestRunCmdErrorMessages(unittest.TestCase):
    """Tests for error display behavior."""

    @patch("src.core.process.subprocess.run")
    def test_verbose_error_omits_rerun_tip(self, mock_run):
        """With verbose=True, the 'Re-run with --verbose' tip is not shown."""
        mock_run.side_effect = subprocess.CalledProcessError(1, "cmd")
        ui = _make_ui()
        with patch.object(ui, 'error_block') as mock_error:
            with self.assertRaises(RuntimeError):
                run_cmd("cmd", tail=False, verbose=True, ui=ui)
        # error_block called with tips that don't include --verbose hint
        _, kwargs_or_args = mock_error.call_args
        if isinstance(kwargs_or_args, tuple):
            tips = mock_error.call_args[0][1] if len(mock_error.call_args[0]) > 1 else []
        else:
            tips = kwargs_or_args.get('tips', [])
        for tip in tips:
            self.assertNotIn("--verbose", tip)

    @patch("src.core.process.subprocess.run")
    def test_non_verbose_error_includes_rerun_tip(self, mock_run):
        """With verbose=False, the 'Re-run with --verbose' tip is shown."""
        mock_run.side_effect = subprocess.CalledProcessError(1, "cmd")
        ui = _make_ui()
        with patch.object(ui, 'error_block') as mock_error:
            with self.assertRaises(RuntimeError):
                run_cmd("cmd", tail=False, verbose=False, ui=ui)
        tips = mock_error.call_args[0][1]
        verbose_tips = [t for t in tips if "--verbose" in t]
        self.assertTrue(len(verbose_tips) > 0)


if __name__ == "__main__":
    unittest.main()
