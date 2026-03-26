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

"""Tests for recipe builder flag propagation."""

import sys
import unittest
from unittest.mock import patch, MagicMock, call

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.recipes.schema import RecipeSource, Recipe
from src.recipes.builder import run_cmake_recipe_step, build_recipe


def _make_recipe(name="testlib", build_steps=None):
    """Helper to create a recipe with a cmake build step."""
    source = RecipeSource(type="git", url=f"https://example.com/{name}.git")
    return Recipe(
        name=name,
        version="1.0",
        source=source,
        dependencies=[],
        source_dir="/src/testlib",
        build_dir="/build/testlib",
        install_dir="/install/testlib",
        build_type="Release",
        build=build_steps or [{"type": "cmake"}],
    )


class TestCMakeStepFlagPropagation(unittest.TestCase):
    """Test that cflags/cxxflags/ldflags propagate to cmake commands."""

    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_cflags_in_configure(self, mock_which, mock_ensure, mock_run):
        """User cflags should appear as CMAKE_C_FLAGS_INIT in configure."""
        recipe = _make_recipe()
        step = {"type": "cmake"}
        run_cmake_recipe_step(
            recipe, step, "/src", "/third-party", "preset",
            cflags="-gz=zstd",
        )
        configure_call = mock_run.call_args_list[0]
        cmd = configure_call[0][0]
        c_flags = [a for a in cmd if "CMAKE_C_FLAGS_INIT" in a]
        self.assertTrue(len(c_flags) > 0, f"CMAKE_C_FLAGS_INIT not found: {cmd}")
        self.assertIn("-gz=zstd", c_flags[0], f"User cflags missing: {c_flags[0]}")
        self.assertIn("-w", c_flags[0], f"Warning suppression missing: {c_flags[0]}")

    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_cxxflags_in_configure(self, mock_which, mock_ensure, mock_run):
        """User cxxflags should appear as CMAKE_CXX_FLAGS_INIT."""
        recipe = _make_recipe()
        step = {"type": "cmake"}
        run_cmake_recipe_step(
            recipe, step, "/src", "/third-party", "preset",
            cxxflags="-gz=zstd -O2",
        )
        configure_call = mock_run.call_args_list[0]
        cmd = configure_call[0][0]
        cxx_flags = [a for a in cmd if "CMAKE_CXX_FLAGS_INIT" in a]
        self.assertTrue(len(cxx_flags) > 0, f"CMAKE_CXX_FLAGS_INIT not found: {cmd}")
        self.assertIn("-gz=zstd -O2", cxx_flags[0], f"User cxxflags missing: {cxx_flags[0]}")
        self.assertIn("-w", cxx_flags[0], f"Warning suppression missing: {cxx_flags[0]}")

    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_ldflags_in_configure(self, mock_which, mock_ensure, mock_run):
        """User ldflags should appear as CMAKE_EXE_LINKER_FLAGS_INIT and CMAKE_SHARED_LINKER_FLAGS_INIT."""
        recipe = _make_recipe()
        step = {"type": "cmake"}
        run_cmake_recipe_step(
            recipe, step, "/src", "/third-party", "preset",
            ldflags="-fuse-ld=lld",
        )
        configure_call = mock_run.call_args_list[0]
        cmd = configure_call[0][0]
        self.assertTrue(
            any("-DCMAKE_EXE_LINKER_FLAGS_INIT=-fuse-ld=lld" in arg for arg in cmd),
            f"CMAKE_EXE_LINKER_FLAGS_INIT not found: {cmd}"
        )
        self.assertTrue(
            any("-DCMAKE_SHARED_LINKER_FLAGS_INIT=-fuse-ld=lld" in arg for arg in cmd),
            f"CMAKE_SHARED_LINKER_FLAGS_INIT not found: {cmd}"
        )

    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_only_warning_suppression_when_no_user_flags(self, mock_which, mock_ensure, mock_run):
        """Only -w warning suppression in FLAGS_INIT when no user flags or sanitizer."""
        recipe = _make_recipe()
        step = {"type": "cmake"}
        run_cmake_recipe_step(
            recipe, step, "/src", "/third-party", "preset",
        )
        configure_call = mock_run.call_args_list[0]
        cmd = configure_call[0][0]
        c_flags = [a for a in cmd if "CMAKE_C_FLAGS_INIT" in a]
        cxx_flags = [a for a in cmd if "CMAKE_CXX_FLAGS_INIT" in a]
        self.assertTrue(len(c_flags) > 0, f"CMAKE_C_FLAGS_INIT expected with -w: {cmd}")
        self.assertEqual(c_flags[0], "-DCMAKE_C_FLAGS_INIT=-w")
        self.assertTrue(len(cxx_flags) > 0, f"CMAKE_CXX_FLAGS_INIT expected with -w: {cmd}")
        self.assertEqual(cxx_flags[0], "-DCMAKE_CXX_FLAGS_INIT=-w")
        # No linker flags when no user ldflags
        self.assertFalse(
            any("LINKER_FLAGS_INIT" in arg for arg in cmd),
            f"Unexpected LINKER_FLAGS_INIT: {cmd}"
        )

    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    @patch("src.recipes.builder.is_windows", return_value=False)
    def test_sanitizer_flags_auto_added(self, mock_win, mock_which, mock_ensure, mock_run):
        """Sanitizer flags should be auto-generated from --sanitizer."""
        recipe = _make_recipe()
        step = {"type": "cmake"}
        run_cmake_recipe_step(
            recipe, step, "/src", "/third-party", "preset",
            sanitizer="address",
        )
        configure_call = mock_run.call_args_list[0]
        cmd = configure_call[0][0]
        c_flags = [a for a in cmd if "CMAKE_C_FLAGS_INIT" in a]
        cxx_flags = [a for a in cmd if "CMAKE_CXX_FLAGS_INIT" in a]
        self.assertTrue(len(c_flags) > 0, "Sanitizer should set CMAKE_C_FLAGS_INIT")
        self.assertIn("-fsanitize=address", c_flags[0])
        self.assertTrue(len(cxx_flags) > 0, "Sanitizer should set CMAKE_CXX_FLAGS_INIT")
        self.assertIn("-fsanitize=address", cxx_flags[0])

    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    @patch("src.recipes.builder.is_windows", return_value=False)
    def test_sanitizer_plus_user_flags_merged(self, mock_win, mock_which, mock_ensure, mock_run):
        """Sanitizer and user flags should be merged together."""
        recipe = _make_recipe()
        step = {"type": "cmake"}
        run_cmake_recipe_step(
            recipe, step, "/src", "/third-party", "preset",
            sanitizer="address",
            cxxflags="-gz=zstd",
        )
        configure_call = mock_run.call_args_list[0]
        cmd = configure_call[0][0]
        cxx_flags = [a for a in cmd if "CMAKE_CXX_FLAGS_INIT" in a]
        self.assertTrue(len(cxx_flags) > 0)
        # Both sanitizer and user flags should be present
        self.assertIn("-fsanitize=address", cxx_flags[0])
        self.assertIn("-gz=zstd", cxx_flags[0])


class TestNeedsLibcxxRuntimes(unittest.TestCase):
    """Test needs_libcxx_runtimes() guard function."""

    def test_clang_asan_needs_runtimes(self):
        """Clang + ASan should trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertTrue(needs_libcxx_runtimes("address", "Clang"))
        self.assertTrue(needs_libcxx_runtimes("asan", "Clang"))

    def test_clang_msan_needs_runtimes(self):
        """Clang + MSan should trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertTrue(needs_libcxx_runtimes("memory", "Clang"))
        self.assertTrue(needs_libcxx_runtimes("msan", "Clang"))

    def test_clang_ubsan_does_not_need_runtimes(self):
        """Clang + UBSan should NOT trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertFalse(needs_libcxx_runtimes("undefined", "Clang"))
        self.assertFalse(needs_libcxx_runtimes("ubsan", "Clang"))

    def test_clang_tsan_does_not_need_runtimes(self):
        """Clang + TSan should NOT trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertFalse(needs_libcxx_runtimes("thread", "Clang"))
        self.assertFalse(needs_libcxx_runtimes("tsan", "Clang"))

    def test_gcc_asan_does_not_need_runtimes(self):
        """GCC + ASan should NOT trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertFalse(needs_libcxx_runtimes("address", "GNU"))

    def test_msvc_does_not_need_runtimes(self):
        """MSVC should NOT trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertFalse(needs_libcxx_runtimes("address", "MSVC"))

    def test_apple_clang_does_not_need_runtimes(self):
        """AppleClang should NOT trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertFalse(needs_libcxx_runtimes("address", "AppleClang"))

    def test_no_sanitizer_does_not_need_runtimes(self):
        """No sanitizer should NOT trigger runtimes build."""
        from src.recipes.builder import needs_libcxx_runtimes
        self.assertFalse(needs_libcxx_runtimes("", "Clang"))


class TestBuildLibcxxRuntimes(unittest.TestCase):
    """Test build_libcxx_runtimes() cmake command generation."""

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_asan_runtimes_configure(self, mock_which, mock_ensure, mock_run, mock_rm):
        """ASan runtimes build should configure with Address sanitizer."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(
            recipe, cc="/usr/bin/clang", cxx="/usr/bin/clang++",
            sanitizer="address",
        )
        # First run_cmd call is configure
        cfg_cmd = mock_run.call_args_list[0][0][0]
        self.assertTrue(any("LLVM_ENABLE_RUNTIMES=libcxx;libcxxabi" in a for a in cfg_cmd))
        self.assertTrue(any("LLVM_USE_SANITIZER=Address" in a for a in cfg_cmd))

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_msan_runtimes_configure(self, mock_which, mock_ensure, mock_run, mock_rm):
        """MSan runtimes build should configure with MemoryWithOrigins."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(
            recipe, cc="/usr/bin/clang", cxx="/usr/bin/clang++",
            sanitizer="memory",
        )
        cfg_cmd = mock_run.call_args_list[0][0][0]
        self.assertTrue(any("LLVM_USE_SANITIZER=MemoryWithOrigins" in a for a in cfg_cmd))

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_runtimes_source_dir(self, mock_which, mock_ensure, mock_run, mock_rm):
        """Runtimes should be built from <source>/runtimes."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(recipe, sanitizer="address")
        cfg_cmd = mock_run.call_args_list[0][0][0]
        # -S should point to source_dir/runtimes
        s_idx = cfg_cmd.index("-S")
        self.assertTrue(cfg_cmd[s_idx + 1].endswith("/runtimes"))

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_runtimes_build_dir_cleaned_up(self, mock_which, mock_ensure, mock_run, mock_rm):
        """Runtimes build directory should be cleaned up after installation."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(recipe, sanitizer="address")
        # remove_dir should be called with the runtimes build dir
        mock_rm.assert_called_once()
        rm_path = mock_rm.call_args[0][0]
        self.assertTrue(rm_path.endswith("-runtimes"))

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_runtimes_install_prefix(self, mock_which, mock_ensure, mock_run, mock_rm):
        """Runtimes should install to the LLVM install prefix."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(recipe, sanitizer="address")
        cfg_cmd = mock_run.call_args_list[0][0][0]
        install_prefix = [a for a in cfg_cmd if "CMAKE_INSTALL_PREFIX" in a]
        self.assertTrue(len(install_prefix) > 0)
        self.assertIn(recipe.install_dir, install_prefix[0])

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    def test_runtimes_three_cmake_steps(self, mock_which, mock_ensure, mock_run, mock_rm):
        """Runtimes build should run configure, build, and install."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(recipe, sanitizer="address")
        # Should have 3 run_cmd calls: configure, build, install
        self.assertEqual(mock_run.call_count, 3)
        # Configure has -S
        self.assertIn("-S", mock_run.call_args_list[0][0][0])
        # Build has --build
        self.assertIn("--build", mock_run.call_args_list[1][0][0])
        # Install has --install
        self.assertIn("--install", mock_run.call_args_list[2][0][0])

    @patch("src.recipes.builder.remove_dir")
    @patch("src.recipes.builder.run_cmd")
    @patch("src.recipes.builder.ensure_dir")
    @patch("shutil.which", return_value="/usr/bin/cmake")
    @patch("src.recipes.builder.is_windows", return_value=True)
    def test_windows_runtimes_libcxx_only(self, mock_win, mock_which, mock_ensure, mock_run, mock_rm):
        """On Windows, only libcxx should be built (not libcxxabi)."""
        from src.recipes.builder import build_libcxx_runtimes
        recipe = _make_recipe(name="llvm")
        build_libcxx_runtimes(recipe, sanitizer="address")
        cfg_cmd = mock_run.call_args_list[0][0][0]
        runtimes_arg = [a for a in cfg_cmd if "LLVM_ENABLE_RUNTIMES" in a][0]
        self.assertIn("libcxx", runtimes_arg)
        self.assertNotIn("libcxxabi", runtimes_arg)


class TestLibcxxRuntimeFlags(unittest.TestCase):
    """Test libcxx_runtime_flags() downstream flag computation."""

    def test_unix_flags(self):
        """Unix flags should include -nostdinc++, -nostdlib++, isystem, rpath, and -lc++abi."""
        from src.recipes.builder import libcxx_runtime_flags
        with patch("src.recipes.builder.is_windows", return_value=False):
            flags = libcxx_runtime_flags("/opt/llvm")
        self.assertIn("-nostdinc++", flags["cxxflags"])
        self.assertIn("-nostdlib++", flags["cxxflags"])
        self.assertIn("-isystem /opt/llvm/include/c++/v1", flags["cxxflags"])
        self.assertIn("-L/opt/llvm/lib", flags["ldflags"])
        self.assertIn("-lc++abi", flags["ldflags"])
        self.assertIn("-lc++", flags["ldflags"])
        self.assertIn("-Wl,-rpath,/opt/llvm/lib", flags["ldflags"])

    def test_windows_flags(self):
        """Windows flags should NOT include -lc++abi or rpath."""
        from src.recipes.builder import libcxx_runtime_flags
        with patch("src.recipes.builder.is_windows", return_value=True):
            flags = libcxx_runtime_flags("/opt/llvm")
        self.assertIn("-nostdinc++", flags["cxxflags"])
        self.assertIn("-L/opt/llvm/lib", flags["ldflags"])
        self.assertIn("-lc++", flags["ldflags"])
        self.assertNotIn("-lc++abi", flags["ldflags"])
        self.assertNotIn("-Wl,-rpath", flags["ldflags"])

    def test_flags_use_install_prefix(self):
        """Flags should use the provided install prefix path."""
        from src.recipes.builder import libcxx_runtime_flags
        with patch("src.recipes.builder.is_windows", return_value=False):
            flags = libcxx_runtime_flags("/custom/path")
        self.assertIn("/custom/path/include/c++/v1", flags["cxxflags"])
        self.assertIn("/custom/path/lib", flags["ldflags"])


class TestBuildRecipeFlagPassthrough(unittest.TestCase):
    """Test that build_recipe passes flags through to step runners."""

    @patch("src.recipes.builder.run_cmake_recipe_step")
    def test_flags_passed_to_cmake_step(self, mock_step):
        """build_recipe should pass cflags/cxxflags/ldflags to cmake step."""
        recipe = _make_recipe()
        build_recipe(
            recipe, "/src", "/third-party", "preset",
            cflags="-Wall",
            cxxflags="-std=c++20",
            ldflags="-fuse-ld=lld",
        )
        mock_step.assert_called_once()
        _, kwargs = mock_step.call_args
        # The flags are positional in the call, check all args
        args = mock_step.call_args
        # build_recipe passes flags positionally
        call_args = args[0] if args[0] else []
        call_kwargs = args[1] if len(args) > 1 else {}
        # The function is called with positional args
        # recipe, step, source_dir, third_party, preset, cc, cxx,
        # build_dir, install_dir, sanitizer, cflags, cxxflags, ldflags, ...
        all_args = list(call_args) + list(call_kwargs.values())
        self.assertIn("-Wall", all_args, "cflags not passed through")
        self.assertIn("-std=c++20", all_args, "cxxflags not passed through")
        self.assertIn("-fuse-ld=lld", all_args, "ldflags not passed through")


if __name__ == "__main__":
    unittest.main()
