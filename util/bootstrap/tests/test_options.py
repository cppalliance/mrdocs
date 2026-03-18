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

"""Tests for InstallOptions dataclass."""

import sys
import unittest

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.core.options import InstallOptions, BUILD_TYPES, SANITIZERS


class TestInstallOptions(unittest.TestCase):
    """Test InstallOptions dataclass."""

    def test_default_construction(self):
        """InstallOptions should construct with defaults."""
        opts = InstallOptions()
        self.assertIsInstance(opts, InstallOptions)

    def test_default_build_type(self):
        """Default build type should be Release."""
        opts = InstallOptions()
        self.assertEqual(opts.build_type, "Release")

    def test_default_build_tests(self):
        """Default build_tests should be True."""
        opts = InstallOptions()
        self.assertTrue(opts.build_tests)

    def test_default_dry_run(self):
        """Default dry_run should be False."""
        opts = InstallOptions()
        self.assertFalse(opts.dry_run)

    def test_default_verbose(self):
        """Default verbose should be False."""
        opts = InstallOptions()
        self.assertFalse(opts.verbose)

    def test_override_build_type(self):
        """Should be able to override build_type."""
        opts = InstallOptions(build_type="Debug")
        self.assertEqual(opts.build_type, "Debug")

    def test_override_compiler(self):
        """Should be able to set compiler paths."""
        opts = InstallOptions(cc="/usr/bin/gcc", cxx="/usr/bin/g++")
        self.assertEqual(opts.cc, "/usr/bin/gcc")
        self.assertEqual(opts.cxx, "/usr/bin/g++")

    def test_override_sanitizer(self):
        """Should be able to set sanitizer."""
        opts = InstallOptions(sanitizer="address")
        self.assertEqual(opts.sanitizer, "address")

    def test_source_dir_is_set(self):
        """source_dir should have a default value."""
        opts = InstallOptions()
        self.assertIsInstance(opts.source_dir, str)
        self.assertTrue(len(opts.source_dir) > 0)

    def test_equality(self):
        """Two InstallOptions with same values should be equal."""
        opts1 = InstallOptions(build_type="Debug", cc="/usr/bin/clang")
        opts2 = InstallOptions(build_type="Debug", cc="/usr/bin/clang")
        self.assertEqual(opts1, opts2)

    def test_inequality(self):
        """Two InstallOptions with different values should not be equal."""
        opts1 = InstallOptions(build_type="Debug")
        opts2 = InstallOptions(build_type="Release")
        self.assertNotEqual(opts1, opts2)

    def test_default_cflags_empty(self):
        """Default cflags should be empty."""
        opts = InstallOptions()
        self.assertEqual(opts.cflags, '')

    def test_default_cxxflags_empty(self):
        """Default cxxflags should be empty."""
        opts = InstallOptions()
        self.assertEqual(opts.cxxflags, '')

    def test_default_ldflags_empty(self):
        """Default ldflags should be empty."""
        opts = InstallOptions()
        self.assertEqual(opts.ldflags, '')

    def test_override_cflags(self):
        """Should be able to set cflags."""
        opts = InstallOptions(cflags="-gz=zstd")
        self.assertEqual(opts.cflags, "-gz=zstd")

    def test_override_cxxflags(self):
        """Should be able to set cxxflags."""
        opts = InstallOptions(cxxflags="-gz=zstd -O2")
        self.assertEqual(opts.cxxflags, "-gz=zstd -O2")

    def test_override_ldflags(self):
        """Should be able to set ldflags."""
        opts = InstallOptions(ldflags="-fuse-ld=lld")
        self.assertEqual(opts.ldflags, "-fuse-ld=lld")


class TestBuildTypes(unittest.TestCase):
    """Test BUILD_TYPES constant."""

    def test_contains_release(self):
        """BUILD_TYPES should contain Release."""
        self.assertIn("Release", BUILD_TYPES)

    def test_contains_debug(self):
        """BUILD_TYPES should contain Debug."""
        self.assertIn("Debug", BUILD_TYPES)

    def test_contains_relwithdebinfo(self):
        """BUILD_TYPES should contain RelWithDebInfo."""
        self.assertIn("RelWithDebInfo", BUILD_TYPES)

    def test_contains_minsizerel(self):
        """BUILD_TYPES should contain MinSizeRel."""
        self.assertIn("MinSizeRel", BUILD_TYPES)

    def test_is_list(self):
        """BUILD_TYPES should be a list."""
        self.assertIsInstance(BUILD_TYPES, list)


class TestSanitizers(unittest.TestCase):
    """Test SANITIZERS constant."""

    def test_contains_address(self):
        """SANITIZERS should contain address."""
        self.assertIn("address", SANITIZERS)

    def test_contains_undefined(self):
        """SANITIZERS should contain undefined."""
        self.assertIn("undefined", SANITIZERS)

    def test_contains_thread(self):
        """SANITIZERS should contain thread."""
        self.assertIn("thread", SANITIZERS)

    def test_contains_memory(self):
        """SANITIZERS should contain memory."""
        self.assertIn("memory", SANITIZERS)

    def test_contains_empty(self):
        """SANITIZERS should contain empty string (no sanitizer)."""
        self.assertIn("", SANITIZERS)


class TestCLIFlagParsing(unittest.TestCase):
    """Test that --cflags, --cxxflags, --ldflags are parsed from CLI."""

    @classmethod
    def setUpClass(cls):
        # Import here to avoid polluting other tests
        from src.__main__ import get_command_line_args
        cls.get_command_line_args = staticmethod(get_command_line_args)

    def test_cflags_parsed(self):
        """--cflags should be captured in parsed args."""
        args = self.get_command_line_args(["--cflags=-gz=zstd"])
        self.assertEqual(args["cflags"], "-gz=zstd")

    def test_cxxflags_parsed(self):
        """--cxxflags should be captured in parsed args."""
        args = self.get_command_line_args(["--cxxflags=-gz=zstd -O2"])
        self.assertEqual(args["cxxflags"], "-gz=zstd -O2")

    def test_ldflags_parsed(self):
        """--ldflags should be captured in parsed args."""
        args = self.get_command_line_args(["--ldflags=-fuse-ld=lld"])
        self.assertEqual(args["ldflags"], "-fuse-ld=lld")

    def test_flags_not_present_when_omitted(self):
        """Flag options should not appear in args dict when omitted."""
        args = self.get_command_line_args(["--build-type", "Debug"])
        self.assertNotIn("cflags", args)
        self.assertNotIn("cxxflags", args)
        self.assertNotIn("ldflags", args)

    def test_all_flags_together(self):
        """All three flag options should work together."""
        args = self.get_command_line_args([
            "--cflags=-Wall",
            "--cxxflags=-std=c++20",
            "--ldflags=-Wl,-rpath,/usr/lib",
        ])
        self.assertEqual(args["cflags"], "-Wall")
        self.assertEqual(args["cxxflags"], "-std=c++20")
        self.assertEqual(args["ldflags"], "-Wl,-rpath,/usr/lib")

    def test_flags_with_sanitizer(self):
        """Flag options should coexist with --sanitizer."""
        args = self.get_command_line_args([
            "--sanitizer", "address",
            "--cxxflags=-gz=zstd",
        ])
        self.assertEqual(args["sanitizer"], "address")
        self.assertEqual(args["cxxflags"], "-gz=zstd")


if __name__ == "__main__":
    unittest.main()
