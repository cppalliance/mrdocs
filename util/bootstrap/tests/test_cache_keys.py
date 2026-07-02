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

"""Tests for CI-compatible cache key generation."""

import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.recipes.cache_keys import (
    generate_cache_key,
    _sanitizer_archive_str,
    detect_compiler_for_cache_key,
)


LLVM_HASH = "77e43ec11cd8fbe1de491118b54de9bba94510a8"


class TestGenerateCacheKey(unittest.TestCase):
    """Test generate_cache_key() produces CI-compatible keys."""

    def test_basic_release_key(self):
        """Basic release key without sanitizer suffix."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
        )
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_debug_build_type(self):
        """Build type should be lowercased."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Debug",
            os_key="macos-15",
        )
        self.assertEqual(key, "llvm-77e43ec-debug-macos-15")

    def test_relwithdebinfo_build_type(self):
        """RelWithDebInfo should be lowercased."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="RelWithDebInfo",
            os_key="ubuntu:24.04",
        )
        self.assertEqual(key, "llvm-77e43ec-relwithdebinfo-ubuntu-24.04")

    def test_colon_replaced_in_os_key(self):
        """Colons in os-key should be replaced with hyphens."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:22.04",
        )
        self.assertNotIn(":", key)
        self.assertIn("ubuntu-22.04", key)

    def test_os_key_without_colons_unchanged(self):
        """OS keys without colons should pass through unchanged."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="macos-15",
        )
        self.assertIn("macos-15", key)

    def test_windows_os_key(self):
        """Windows runner name should work as os-key."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="windows-2022",
        )
        self.assertEqual(key, "llvm-77e43ec-release-windows-2022")

    def test_hash_truncated_to_7_chars(self):
        """Only first 7 characters of hash should be used."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
        )
        self.assertIn("77e43ec", key)
        # Full hash should NOT appear
        self.assertNotIn("77e43ec1", key)

    def test_clang_asan_suffix(self):
        """Clang + ASan should add -clang-{version}-ASan suffix."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
            sanitizer="address",
        )
        self.assertEqual(
            key, "llvm-77e43ec-release-ubuntu-24.04-clang-19-ASan"
        )

    def test_clang_asan_short_name(self):
        """Short sanitizer name 'asan' should also trigger suffix."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
            sanitizer="asan",
        )
        self.assertEqual(
            key, "llvm-77e43ec-release-ubuntu-24.04-clang-19-ASan"
        )

    def test_clang_msan_suffix(self):
        """Clang + MSan should add suffix with empty sanitizer string."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
            sanitizer="memory",
        )
        # MSan has no archive-sanitizer-str in CI template, so empty after last dash
        self.assertEqual(
            key, "llvm-77e43ec-release-ubuntu-24.04-clang-19-"
        )

    def test_gcc_asan_no_suffix(self):
        """GCC + ASan should NOT add compiler suffix."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="gcc",
            compiler_version="14",
            sanitizer="address",
        )
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_clang_ubsan_no_suffix(self):
        """Clang + UBSan should NOT add suffix (only asan/msan trigger it)."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
            sanitizer="undefined",
        )
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_clang_tsan_no_suffix(self):
        """Clang + TSan should NOT add suffix."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
            sanitizer="thread",
        )
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_no_compiler_no_suffix(self):
        """No compiler specified should produce no suffix."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            sanitizer="address",
        )
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_no_sanitizer_no_suffix(self):
        """No sanitizer should produce no suffix even with clang."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
        )
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_non_llvm_recipe(self):
        """Cache keys should work for any recipe, not just llvm."""
        key = generate_cache_key(
            recipe_name="libxml2",
            recipe_hash="abc1234567890",
            build_type="Release",
            os_key="ubuntu:24.04",
        )
        self.assertEqual(key, "libxml2-abc1234-release-ubuntu-24.04")


class TestSanitizerArchiveStr(unittest.TestCase):
    """Test _sanitizer_archive_str() mapping."""

    def test_address_maps_to_asan(self):
        self.assertEqual(_sanitizer_archive_str("address"), "ASan")

    def test_undefined_maps_to_ubsan(self):
        self.assertEqual(_sanitizer_archive_str("undefined"), "UBSan")

    def test_memory_maps_to_empty(self):
        self.assertEqual(_sanitizer_archive_str("memory"), "")

    def test_thread_maps_to_empty(self):
        self.assertEqual(_sanitizer_archive_str("thread"), "")

    def test_unknown_maps_to_empty(self):
        self.assertEqual(_sanitizer_archive_str("unknown"), "")


class TestDetectCompilerForCacheKey(unittest.TestCase):
    """Test detect_compiler_for_cache_key() compiler detection."""

    def test_clang_with_version_in_path(self):
        """clang-19 should detect as clang version 19."""
        name, version = detect_compiler_for_cache_key("/usr/bin/clang-19")
        self.assertEqual(name, "clang")
        self.assertEqual(version, "19")

    def test_clang_without_version(self):
        """Plain 'clang' should detect as clang with empty or probed version."""
        with patch("subprocess.run", side_effect=FileNotFoundError):
            name, version = detect_compiler_for_cache_key("/usr/bin/clang")
        self.assertEqual(name, "clang")

    def test_gcc_with_version_in_path(self):
        """gcc-14 should detect as gcc version 14."""
        name, version = detect_compiler_for_cache_key("/usr/bin/gcc-14")
        self.assertEqual(name, "gcc")
        self.assertEqual(version, "14")

    def test_gpp_with_version_in_path(self):
        """g++-14 should detect as gcc version 14."""
        name, version = detect_compiler_for_cache_key("/usr/bin/g++-14")
        self.assertEqual(name, "gcc")
        self.assertEqual(version, "14")

    def test_msvc_cl(self):
        """cl.exe should detect as msvc."""
        with patch("subprocess.run", side_effect=FileNotFoundError):
            name, version = detect_compiler_for_cache_key("cl.exe")
        self.assertEqual(name, "msvc")

    def test_empty_cc_returns_empty(self):
        """Empty cc should return empty strings."""
        name, version = detect_compiler_for_cache_key("")
        self.assertEqual(name, "")
        self.assertEqual(version, "")

    def test_clang_version_from_dumpversion(self):
        """When version not in path, should try -dumpversion."""
        mock_result = type("Result", (), {"returncode": 0, "stdout": "19.1.0\n"})()
        with patch("subprocess.run", return_value=mock_result):
            name, version = detect_compiler_for_cache_key("/usr/bin/clang")
        self.assertEqual(name, "clang")
        self.assertEqual(version, "19")


class TestCacheKeyMatchesCI(unittest.TestCase):
    """
    Verify that generated keys match known CI key examples.

    These tests use the actual LLVM hash and CI matrix values
    to ensure exact compatibility.
    """

    def test_release_ubuntu_gcc(self):
        """Standard gcc release on ubuntu:24.04."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="gcc",
            compiler_version="14",
        )
        # gcc builds never get the compiler suffix
        self.assertEqual(key, "llvm-77e43ec-release-ubuntu-24.04")

    def test_release_ubuntu_clang_asan(self):
        """Clang ASan release on ubuntu:24.04."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="ubuntu:24.04",
            compiler="clang",
            compiler_version="19",
            sanitizer="address",
        )
        self.assertEqual(
            key, "llvm-77e43ec-release-ubuntu-24.04-clang-19-ASan"
        )

    def test_release_windows(self):
        """MSVC release on windows-2022."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="windows-2022",
            compiler="msvc",
            compiler_version="14.42",
        )
        self.assertEqual(key, "llvm-77e43ec-release-windows-2022")

    def test_release_macos(self):
        """apple-clang release on macos-15."""
        key = generate_cache_key(
            recipe_name="llvm",
            recipe_hash=LLVM_HASH,
            build_type="Release",
            os_key="macos-15",
            compiler="apple-clang",
            compiler_version="16",
        )
        # apple-clang never gets suffix (only "clang" does)
        self.assertEqual(key, "llvm-77e43ec-release-macos-15")


if __name__ == "__main__":
    unittest.main()
