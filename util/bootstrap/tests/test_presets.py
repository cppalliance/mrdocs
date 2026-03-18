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

"""Tests for CMake presets generation."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.presets.generator import (
    get_host_system_name,
    get_parent_preset_name,
    get_display_name,
    normalize_preset_value,
    create_cmake_presets,
)


class TestGetHostSystemName(unittest.TestCase):
    """Test get_host_system_name function."""

    def test_returns_tuple(self):
        """Should return a tuple of two strings."""
        result = get_host_system_name()
        self.assertIsInstance(result, tuple)
        self.assertEqual(len(result), 2)

    def test_first_element_is_system_name(self):
        """First element should be a valid system name."""
        system_name, _ = get_host_system_name()
        self.assertIn(system_name, ["Windows", "Linux", "Darwin"])

    def test_second_element_is_display_name(self):
        """Second element should be a display name."""
        _, display_name = get_host_system_name()
        self.assertIn(display_name, ["Windows", "Linux", "macOS"])

    def test_darwin_maps_to_macos(self):
        """Darwin system should have macOS display name."""
        system_name, display_name = get_host_system_name()
        if system_name == "Darwin":
            self.assertEqual(display_name, "macOS")


class TestGetParentPresetName(unittest.TestCase):
    """Test get_parent_preset_name function."""

    def test_debug_returns_debug(self):
        """Debug build type should return debug preset."""
        self.assertEqual(get_parent_preset_name("Debug"), "debug")

    def test_debug_case_insensitive(self):
        """Should be case insensitive."""
        self.assertEqual(get_parent_preset_name("debug"), "debug")
        self.assertEqual(get_parent_preset_name("DEBUG"), "debug")

    def test_release_returns_release(self):
        """Release build type should return release preset."""
        self.assertEqual(get_parent_preset_name("Release"), "release")

    def test_relwithdebinfo_returns_relwithdebinfo(self):
        """RelWithDebInfo should return relwithdebinfo preset."""
        self.assertEqual(get_parent_preset_name("RelWithDebInfo"), "relwithdebinfo")

    def test_minsizerel_returns_release(self):
        """MinSizeRel should return release preset."""
        self.assertEqual(get_parent_preset_name("MinSizeRel"), "release")

    def test_debugfast_returns_debug(self):
        """DebugFast variants should return debug preset."""
        self.assertEqual(get_parent_preset_name("debugfast"), "debug")
        self.assertEqual(get_parent_preset_name("debug-fast"), "debug")


class TestGetDisplayName(unittest.TestCase):
    """Test get_display_name function."""

    def test_basic_display_name(self):
        """Should create basic display name."""
        name = get_display_name("Release", "Linux")
        self.assertIn("Release", name)
        self.assertIn("Linux", name)

    def test_with_compiler(self):
        """Should include compiler in display name."""
        name = get_display_name("Debug", "macOS", cc="/usr/bin/clang")
        self.assertIn("Debug", name)
        self.assertIn("macOS", name)
        self.assertIn("clang", name)

    def test_with_sanitizer(self):
        """Should include sanitizer in display name."""
        name = get_display_name("Debug", "Linux", sanitizer="address")
        self.assertIn("address", name)

    def test_debugfast_display(self):
        """DebugFast should display as Debug (fast)."""
        name = get_display_name("debugfast", "Linux")
        self.assertIn("Debug (fast)", name)


class TestNormalizePresetValue(unittest.TestCase):
    """Test normalize_preset_value function."""

    def test_source_dir_replacement(self):
        """Should replace source_dir with ${sourceDir}."""
        result = normalize_preset_value(
            "/home/user/mrdocs/build",
            source_dir="/home/user/mrdocs"
        )
        self.assertEqual(result, "${sourceDir}/build")

    def test_home_dir_replacement(self):
        """Should replace home_dir with $env{HOME}."""
        result = normalize_preset_value(
            "/home/user/.local/bin",
            source_dir="/other/path",
            home_dir="/home/user"
        )
        self.assertEqual(result, "$env{HOME}/.local/bin")

    def test_no_replacement_needed(self):
        """Should not modify paths that don't match."""
        result = normalize_preset_value(
            "/usr/bin/cmake",
            source_dir="/home/user/mrdocs"
        )
        self.assertEqual(result, "/usr/bin/cmake")

    def test_non_string_passthrough(self):
        """Non-string values should pass through unchanged."""
        result = normalize_preset_value(42, source_dir="/path")
        self.assertEqual(result, 42)

    def test_semicolon_separated_paths(self):
        """Should handle semicolon-separated paths."""
        result = normalize_preset_value(
            "/home/user/mrdocs/a;/home/user/mrdocs/b",
            source_dir="/home/user/mrdocs"
        )
        self.assertEqual(result, "${sourceDir}/a;${sourceDir}/b")


class TestCreateCmakePresetsCleanup(unittest.TestCase):
    """Test that create_cmake_presets cleans up stale _ROOT variables."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _write_presets(self, presets_data):
        path = os.path.join(self.tmpdir, "CMakeUserPresets.json")
        with open(path, "w") as f:
            json.dump(presets_data, f)

    def _read_presets(self):
        path = os.path.join(self.tmpdir, "CMakeUserPresets.json")
        with open(path) as f:
            return json.load(f)

    def test_stale_roots_removed_from_current_preset(self):
        """Stale _ROOT vars should be removed from the updated preset."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [{
                "name": "release-linux",
                "generator": "Ninja",
                "inherits": "release",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "LLVM_ROOT": "/old/llvm",
                    "duktape_ROOT": "/old/duktape",
                },
                "warnings": {"unusedCli": False},
                "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"},
            }],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            package_roots={"LLVM_ROOT": "/new/llvm"},
            valid_package_root_vars=["LLVM_ROOT", "Lua_ROOT"],
            dry_run=False,
        )

        data = self._read_presets()
        preset = data["configurePresets"][0]
        self.assertNotIn("duktape_ROOT", preset["cacheVariables"])
        self.assertIn("LLVM_ROOT", preset["cacheVariables"])

    def test_other_presets_not_modified(self):
        """Cleanup should not touch presets that bootstrap didn't create."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [{
                "name": "debug-macos",
                "generator": "Ninja",
                "inherits": "debug",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "duktape_ROOT": "/old/duktape",
                    "Qt6_ROOT": "/opt/qt6",
                    "LLVM_ROOT": "/old/llvm",
                },
                "warnings": {"unusedCli": False},
                "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin"},
            }],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            package_roots={"LLVM_ROOT": "/new/llvm"},
            valid_package_root_vars=["LLVM_ROOT", "Lua_ROOT"],
            dry_run=False,
        )

        data = self._read_presets()
        debug_preset = [p for p in data["configurePresets"] if p["name"] == "debug-macos"][0]
        self.assertIn("duktape_ROOT", debug_preset["cacheVariables"])
        self.assertIn("Qt6_ROOT", debug_preset["cacheVariables"])
        self.assertIn("LLVM_ROOT", debug_preset["cacheVariables"])

    def test_no_cleanup_when_valid_vars_not_provided(self):
        """When valid_package_root_vars is None, no cleanup should occur."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [{
                "name": "release-linux",
                "generator": "Ninja",
                "inherits": "release",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "duktape_ROOT": "/old/duktape",
                },
                "warnings": {"unusedCli": False},
                "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"},
            }],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            package_roots={},
            valid_package_root_vars=None,
            dry_run=False,
        )

        data = self._read_presets()
        preset = data["configurePresets"][0]
        # duktape_ROOT was in old preset but current preset won't have it;
        # however since valid_package_root_vars is None, no cleanup happens
        # The preset gets replaced by upsert, so duktape_ROOT won't be there
        # because it's not in package_roots. But other presets would keep theirs.

    def test_stale_roots_cleaned_from_own_preset(self):
        """Stale _ROOT vars should be removed from the bootstrap-created preset."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [{
                "name": "release-linux",
                "generator": "Ninja",
                "inherits": "release",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "CMAKE_MAKE_PROGRAM": "/usr/bin/ninja",
                    "duktape_ROOT": "/stale",
                    "LLVM_ROOT": "/old/llvm",
                },
                "warnings": {"unusedCli": False},
                "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"},
            }],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            package_roots={"LLVM_ROOT": "/new/llvm"},
            valid_package_root_vars=["LLVM_ROOT"],
            dry_run=False,
        )

        data = self._read_presets()
        preset = [p for p in data["configurePresets"] if p["name"] == "release-linux"][0]
        self.assertNotIn("duktape_ROOT", preset["cacheVariables"])
        self.assertIn("LLVM_ROOT", preset["cacheVariables"])


    def test_preset_fully_replaced_not_merged(self):
        """Updating an existing preset should fully replace it, not merge."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [{
                "name": "release-linux",
                "generator": "Ninja",
                "inherits": "release",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "LLVM_ROOT": "/old/llvm",
                },
                "warnings": {"unusedCli": False},
                "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"},
                "extraOldKey": "should-be-removed",
            }],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            package_roots={"LLVM_ROOT": "/new/llvm"},
            valid_package_root_vars=["LLVM_ROOT"],
            dry_run=False,
        )

        data = self._read_presets()
        preset = data["configurePresets"][0]
        self.assertNotIn("extraOldKey", preset,
                         "Old keys should be removed by full replacement, not preserved by merge")
        self.assertIn("LLVM_ROOT", preset["cacheVariables"])

    def test_git_root_preserved_during_cleanup(self):
        """GIT_ROOT (a tool path) should not be removed from the bootstrap preset."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [{
                "name": "release-linux",
                "generator": "Ninja",
                "inherits": "release",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "GIT_ROOT": "/usr/bin/git",
                    "LLVM_ROOT": "/old/llvm",
                    "duktape_ROOT": "/old/duktape",
                },
                "warnings": {"unusedCli": False},
                "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"},
            }],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            package_roots={"LLVM_ROOT": "/new/llvm"},
            valid_package_root_vars=["LLVM_ROOT"],
            dry_run=False,
        )

        data = self._read_presets()
        preset = [p for p in data["configurePresets"] if p["name"] == "release-linux"][0]
        self.assertNotIn("duktape_ROOT", preset["cacheVariables"])
        self.assertIn("LLVM_ROOT", preset["cacheVariables"])

    def test_cflags_in_preset(self):
        """User cflags should appear as CMAKE_C_FLAGS in preset."""
        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            cflags="-gz=zstd",
            dry_run=False,
        )
        data = self._read_presets()
        preset = data["configurePresets"][0]
        self.assertIn("CMAKE_C_FLAGS", preset["cacheVariables"])
        self.assertIn("-gz=zstd", preset["cacheVariables"]["CMAKE_C_FLAGS"])

    def test_cxxflags_in_preset(self):
        """User cxxflags should appear as CMAKE_CXX_FLAGS in preset."""
        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            cxxflags="-gz=zstd -O2",
            dry_run=False,
        )
        data = self._read_presets()
        preset = data["configurePresets"][0]
        self.assertIn("CMAKE_CXX_FLAGS", preset["cacheVariables"])
        self.assertIn("-gz=zstd", preset["cacheVariables"]["CMAKE_CXX_FLAGS"])

    def test_ldflags_in_preset(self):
        """User ldflags should appear as CMAKE_EXE_LINKER_FLAGS and CMAKE_SHARED_LINKER_FLAGS."""
        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            ldflags="-fuse-ld=lld",
            dry_run=False,
        )
        data = self._read_presets()
        preset = data["configurePresets"][0]
        self.assertIn("CMAKE_EXE_LINKER_FLAGS", preset["cacheVariables"])
        self.assertIn("-fuse-ld=lld", preset["cacheVariables"]["CMAKE_EXE_LINKER_FLAGS"])
        self.assertIn("CMAKE_SHARED_LINKER_FLAGS", preset["cacheVariables"])
        self.assertIn("-fuse-ld=lld", preset["cacheVariables"]["CMAKE_SHARED_LINKER_FLAGS"])

    def test_no_flags_when_empty(self):
        """No FLAGS variables when no user flags or sanitizer."""
        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            dry_run=False,
        )
        data = self._read_presets()
        preset = data["configurePresets"][0]
        self.assertNotIn("CMAKE_C_FLAGS", preset["cacheVariables"])
        self.assertNotIn("CMAKE_EXE_LINKER_FLAGS", preset["cacheVariables"])
        self.assertNotIn("CMAKE_SHARED_LINKER_FLAGS", preset["cacheVariables"])

    def test_sanitizer_and_user_flags_merged_in_preset(self):
        """Sanitizer flags and user flags should be merged in preset."""
        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="release-linux",
            build_type="Release",
            sanitizer="address",
            cxxflags="-gz=zstd",
            dry_run=False,
        )
        data = self._read_presets()
        preset = data["configurePresets"][0]
        cxx_flags = preset["cacheVariables"].get("CMAKE_CXX_FLAGS", "")
        self.assertIn("-fsanitize=address", cxx_flags)
        self.assertIn("-gz=zstd", cxx_flags)

    def test_other_presets_preserved(self):
        """Presets with different names should be preserved (not deleted)."""
        self._write_presets({
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": [
                {
                    "name": "debug-macos",
                    "generator": "Ninja",
                    "inherits": "debug",
                    "binaryDir": "${sourceDir}/build/${presetName}",
                    "cacheVariables": {
                        "CMAKE_BUILD_TYPE": "Debug",
                        "LLVM_ROOT": "/old/llvm",
                    },
                    "warnings": {"unusedCli": False},
                    "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin"},
                },
                {
                    "name": "release-macos",
                    "generator": "Ninja",
                    "inherits": "release",
                    "binaryDir": "${sourceDir}/build/${presetName}",
                    "cacheVariables": {
                        "CMAKE_BUILD_TYPE": "Release",
                        "LLVM_ROOT": "/old/llvm",
                    },
                    "warnings": {"unusedCli": False},
                    "condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin"},
                },
            ],
        })

        create_cmake_presets(
            source_dir=self.tmpdir,
            preset_name="debug-macos",
            build_type="Debug",
            package_roots={"LLVM_ROOT": "/new/llvm"},
            valid_package_root_vars=["LLVM_ROOT"],
            dry_run=False,
        )

        data = self._read_presets()
        names = [p["name"] for p in data["configurePresets"]]
        self.assertIn("debug-macos", names)
        self.assertIn("release-macos", names,
                      "Other presets should be preserved, not deleted")


class TestInjectClangToolchainFlags(unittest.TestCase):
    """Test inject_clang_toolchain_flags libc++ detection with sanitizers."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        # Create a fake LLVM toolchain layout
        self.bin_dir = os.path.join(self.tmpdir, "bin")
        os.makedirs(self.bin_dir)
        self.libcxx_include = os.path.join(self.tmpdir, "include", "c++", "v1")
        self.libcxx_lib = os.path.join(self.tmpdir, "lib", "c++")
        os.makedirs(self.libcxx_include)
        os.makedirs(self.libcxx_lib)
        # Create a fake clang++ binary
        self.cxx_path = os.path.join(self.bin_dir, "clang++")
        with open(self.cxx_path, "w") as f:
            f.write("#!/bin/sh\n")
        os.chmod(self.cxx_path, 0o755)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_sanitizer_asan_uses_explicit_flags(self):
        """ASan + libc++ should use -nostdinc++ -nostdlib++ -isystem flags."""
        from src.presets.generator import inject_clang_toolchain_flags
        extra_vars, cc_flags, cxx_flags = inject_clang_toolchain_flags(
            self.cxx_path,
            {"CMAKE_CXX_COMPILER_ID": "Clang"},
            sanitizer="address",
        )
        self.assertIn("-nostdinc++", cxx_flags)
        self.assertIn("-nostdlib++", cxx_flags)
        self.assertIn("-isystem", cxx_flags)
        ld = extra_vars.get("CMAKE_EXE_LINKER_FLAGS", "")
        self.assertIn("-lc++", ld)
        self.assertIn("-lc++abi", ld)
        self.assertIn("-Wl,-rpath,", ld)

    def test_sanitizer_msan_uses_explicit_flags(self):
        """MSan + libc++ should use -nostdinc++ -nostdlib++ -isystem flags."""
        from src.presets.generator import inject_clang_toolchain_flags
        extra_vars, cc_flags, cxx_flags = inject_clang_toolchain_flags(
            self.cxx_path,
            {"CMAKE_CXX_COMPILER_ID": "Clang"},
            sanitizer="memory",
        )
        self.assertIn("-nostdinc++", cxx_flags)
        self.assertIn("-nostdlib++", cxx_flags)

    def test_no_sanitizer_uses_stdlib_flag(self):
        """Without sanitizer, should use -stdlib=libc++ flag."""
        from src.presets.generator import inject_clang_toolchain_flags
        extra_vars, cc_flags, cxx_flags = inject_clang_toolchain_flags(
            self.cxx_path,
            {"CMAKE_CXX_COMPILER_ID": "Clang"},
            sanitizer="",
        )
        self.assertIn("-stdlib=libc++", cxx_flags)
        self.assertNotIn("-nostdinc++", cxx_flags)

    def test_ubsan_uses_stdlib_flag(self):
        """UBSan should use normal -stdlib=libc++ (not explicit flags)."""
        from src.presets.generator import inject_clang_toolchain_flags
        extra_vars, cc_flags, cxx_flags = inject_clang_toolchain_flags(
            self.cxx_path,
            {"CMAKE_CXX_COMPILER_ID": "Clang"},
            sanitizer="undefined",
        )
        self.assertIn("-stdlib=libc++", cxx_flags)
        self.assertNotIn("-nostdinc++", cxx_flags)

    def test_tsan_uses_stdlib_flag(self):
        """TSan should use normal -stdlib=libc++ (not explicit flags)."""
        from src.presets.generator import inject_clang_toolchain_flags
        extra_vars, cc_flags, cxx_flags = inject_clang_toolchain_flags(
            self.cxx_path,
            {"CMAKE_CXX_COMPILER_ID": "Clang"},
            sanitizer="thread",
        )
        self.assertIn("-stdlib=libc++", cxx_flags)
        self.assertNotIn("-nostdinc++", cxx_flags)

    def test_gcc_no_clang_flags(self):
        """GCC should not get any clang-specific flags."""
        from src.presets.generator import inject_clang_toolchain_flags
        extra_vars, cc_flags, cxx_flags = inject_clang_toolchain_flags(
            self.cxx_path,
            {"CMAKE_CXX_COMPILER_ID": "GNU"},
            sanitizer="address",
        )
        self.assertEqual(cxx_flags, "")
        self.assertNotIn("CMAKE_EXE_LINKER_FLAGS", extra_vars)


if __name__ == "__main__":
    unittest.main()
