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

"""Tests for filesystem utilities."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

from src.core.filesystem import (
    ensure_dir,
    remove_dir,
    write_text,
    is_executable,
    is_non_empty_dir,
    load_json_file,
)


class TestEnsureDir(unittest.TestCase):
    """Test ensure_dir function."""

    def test_creates_directory(self):
        """ensure_dir should create a directory that doesn't exist."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_dir = os.path.join(tmpdir, "new_subdir")
            self.assertFalse(os.path.exists(new_dir))
            ensure_dir(new_dir)
            self.assertTrue(os.path.isdir(new_dir))

    def test_creates_nested_directories(self):
        """ensure_dir should create nested directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            nested = os.path.join(tmpdir, "a", "b", "c")
            ensure_dir(nested)
            self.assertTrue(os.path.isdir(nested))

    def test_existing_directory_ok(self):
        """ensure_dir should not fail if directory exists."""
        with tempfile.TemporaryDirectory() as tmpdir:
            ensure_dir(tmpdir)  # Should not raise
            self.assertTrue(os.path.isdir(tmpdir))

    def test_dry_run_does_not_create(self):
        """ensure_dir with dry_run should not create directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_dir = os.path.join(tmpdir, "should_not_exist")
            ensure_dir(new_dir, dry_run=True)
            self.assertFalse(os.path.exists(new_dir))


class TestRemoveDir(unittest.TestCase):
    """Test remove_dir function."""

    def test_removes_directory(self):
        """remove_dir should remove an existing directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            to_remove = os.path.join(tmpdir, "to_remove")
            os.makedirs(to_remove)
            self.assertTrue(os.path.exists(to_remove))
            remove_dir(to_remove)
            self.assertFalse(os.path.exists(to_remove))

    def test_removes_directory_with_contents(self):
        """remove_dir should remove directory and its contents."""
        with tempfile.TemporaryDirectory() as tmpdir:
            to_remove = os.path.join(tmpdir, "to_remove")
            os.makedirs(to_remove)
            with open(os.path.join(to_remove, "file.txt"), "w") as f:
                f.write("content")
            remove_dir(to_remove)
            self.assertFalse(os.path.exists(to_remove))

    def test_nonexistent_directory_ok(self):
        """remove_dir should not fail if directory doesn't exist."""
        remove_dir("/nonexistent/path/that/does/not/exist")  # Should not raise


class TestWriteText(unittest.TestCase):
    """Test write_text function."""

    def test_writes_file(self):
        """write_text should write content to file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, "test.txt")
            write_text(filepath, "hello world")
            with open(filepath) as f:
                self.assertEqual(f.read(), "hello world")

    def test_creates_parent_directories(self):
        """write_text should create parent directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, "a", "b", "test.txt")
            write_text(filepath, "nested content")
            self.assertTrue(os.path.exists(filepath))

    def test_dry_run_does_not_write(self):
        """write_text with dry_run should not write file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, "should_not_exist.txt")
            write_text(filepath, "content", dry_run=True)
            self.assertFalse(os.path.exists(filepath))


class TestIsExecutable(unittest.TestCase):
    """Test is_executable function."""

    def test_nonexistent_file(self):
        """is_executable should return False for nonexistent file."""
        self.assertFalse(is_executable("/nonexistent/file"))

    def test_empty_path(self):
        """is_executable should return False for empty path."""
        self.assertFalse(is_executable(""))

    def test_none_path(self):
        """is_executable should return False for None."""
        self.assertFalse(is_executable(None))

    def test_directory_not_executable(self):
        """is_executable should return False for directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            self.assertFalse(is_executable(tmpdir))


class TestIsNonEmptyDir(unittest.TestCase):
    """Test is_non_empty_dir function."""

    def test_empty_directory(self):
        """is_non_empty_dir should return False for empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            empty = os.path.join(tmpdir, "empty")
            os.makedirs(empty)
            self.assertFalse(is_non_empty_dir(empty))

    def test_non_empty_directory(self):
        """is_non_empty_dir should return True for non-empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            with open(os.path.join(tmpdir, "file.txt"), "w") as f:
                f.write("content")
            self.assertTrue(is_non_empty_dir(tmpdir))

    def test_nonexistent_path(self):
        """is_non_empty_dir should return False for nonexistent path."""
        self.assertFalse(is_non_empty_dir("/nonexistent/path"))

    def test_file_path(self):
        """is_non_empty_dir should return False for file path."""
        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, "file.txt")
            with open(filepath, "w") as f:
                f.write("content")
            self.assertFalse(is_non_empty_dir(filepath))


class TestLoadJsonFile(unittest.TestCase):
    """Test load_json_file function."""

    def test_loads_valid_json(self):
        """load_json_file should parse valid JSON."""
        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, "test.json")
            data = {"key": "value", "number": 42}
            with open(filepath, "w") as f:
                json.dump(data, f)
            result = load_json_file(filepath)
            self.assertEqual(result, data)

    def test_nonexistent_file_returns_none(self):
        """load_json_file should return None for nonexistent file."""
        result = load_json_file("/nonexistent/file.json")
        self.assertIsNone(result)

    def test_invalid_json_returns_none(self):
        """load_json_file should return None for invalid JSON."""
        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, "invalid.json")
            with open(filepath, "w") as f:
                f.write("not valid json {{{")
            result = load_json_file(filepath)
            self.assertIsNone(result)


if __name__ == "__main__":
    unittest.main()
