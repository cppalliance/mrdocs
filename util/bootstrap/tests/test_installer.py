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

"""Tests for installer.py pure logic methods."""

import os
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.installer import MrDocsInstaller


def _make_installer(**cmd_line_overrides):
    """Create an installer with non-interactive defaults for testing."""
    args = {"non_interactive": True, "plain_ui": True}
    args.update(cmd_line_overrides)
    with tempfile.TemporaryDirectory() as tmp:
        inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
    return inst


class TestExpandPlaceholders(unittest.TestCase):
    """Tests for _expand_placeholders covering all transform types."""

    def _make(self, **kwargs):
        args = {"non_interactive": True, "plain_ui": True}
        args.update(kwargs)
        with tempfile.TemporaryDirectory() as tmp:
            inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
        return inst

    def test_no_placeholders(self):
        inst = self._make()
        self.assertEqual(inst._expand_placeholders("hello"), "hello")

    def test_empty_string(self):
        inst = self._make()
        self.assertEqual(inst._expand_placeholders(""), "")

    def test_lower_transform(self):
        inst = self._make(build_type="Release")
        inst.options.build_type = "Release"
        result = inst._expand_placeholders("<build_type:lower>")
        self.assertEqual(result, "release")

    def test_upper_transform(self):
        inst = self._make(build_type="Release")
        inst.options.build_type = "Release"
        result = inst._expand_placeholders("<build_type:upper>")
        self.assertEqual(result, "RELEASE")

    def test_basename_transform(self):
        inst = self._make(cc="/usr/bin/gcc-15")
        inst.options.cc = "/usr/bin/gcc-15"
        result = inst._expand_placeholders("<cc:basename>")
        self.assertEqual(result, "gcc-15")

    def test_basename_with_extension(self):
        inst = self._make(cc="/usr/bin/gcc.exe")
        inst.options.cc = "/usr/bin/gcc.exe"
        result = inst._expand_placeholders("<cc:basename>")
        self.assertEqual(result, "gcc")

    def test_literal_string(self):
        inst = self._make()
        result = inst._expand_placeholders('<"hello">')
        self.assertEqual(result, "hello")

    def test_os_placeholder(self):
        inst = self._make()
        with patch("src.installer.get_os_name", return_value="Linux"):
            result = inst._expand_placeholders("<os>")
        self.assertEqual(result, "Linux")

    def test_if_conditional_true(self):
        inst = self._make(cc="/usr/bin/gcc")
        inst.options.cc = "/usr/bin/gcc"
        result = inst._expand_placeholders('<"-":if(cc)>')
        self.assertEqual(result, "-")

    def test_if_conditional_false(self):
        inst = self._make()
        inst.options.cc = ""
        result = inst._expand_placeholders('<"-":if(cc)>')
        self.assertEqual(result, "")

    def test_unknown_attribute_returns_empty(self):
        inst = self._make()
        result = inst._expand_placeholders("<nonexistent_attr>")
        self.assertEqual(result, "")

    def test_lower_on_empty_value(self):
        inst = self._make()
        inst.options.sanitizer = ""
        result = inst._expand_placeholders("<sanitizer:lower>")
        self.assertEqual(result, "")

    def test_combined_template(self):
        inst = self._make(build_type="Debug")
        inst.options.build_type = "Debug"
        inst.options.cc = "/usr/bin/gcc-15"
        with patch("src.installer.get_os_name", return_value="Linux"):
            result = inst._expand_placeholders(
                "<build_type:lower>-<os:lower><\"-\":if(cc)><cc:basename>"
            )
        self.assertEqual(result, "debug-linux-gcc-15")


class TestIsAbiCompatible(unittest.TestCase):
    """Tests for is_abi_compatible with compatible and incompatible pairs."""

    def setUp(self):
        self.inst = _make_installer()

    def test_same_type(self):
        self.assertTrue(self.inst.is_abi_compatible("Release", "Release"))
        self.assertTrue(self.inst.is_abi_compatible("Debug", "Debug"))

    def test_debug_types_compatible(self):
        self.assertTrue(self.inst.is_abi_compatible("Debug", "DebugFast"))
        self.assertTrue(self.inst.is_abi_compatible("DebugFast", "Debug"))

    def test_release_types_compatible(self):
        self.assertTrue(self.inst.is_abi_compatible("Release", "RelWithDebInfo"))
        self.assertTrue(self.inst.is_abi_compatible("MinSizeRel", "Release"))
        self.assertTrue(self.inst.is_abi_compatible("OptimizedDebug", "Release"))

    def test_debug_release_incompatible(self):
        self.assertFalse(self.inst.is_abi_compatible("Debug", "Release"))
        self.assertFalse(self.inst.is_abi_compatible("Release", "Debug"))

    def test_case_insensitive(self):
        self.assertTrue(self.inst.is_abi_compatible("debug", "DEBUG"))
        self.assertTrue(self.inst.is_abi_compatible("release", "RELEASE"))

    def test_debug_fast_hyphenated(self):
        self.assertTrue(self.inst.is_abi_compatible("debug-fast", "Debug"))


class TestPromptValidatedOption(unittest.TestCase):
    """Tests for prompt_validated_option non-interactive path."""

    def _make(self, **kwargs):
        args = {"non_interactive": True, "plain_ui": True}
        args.update(kwargs)
        with tempfile.TemporaryDirectory() as tmp:
            inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
        return inst

    def test_returns_default_non_interactive(self):
        inst = self._make(build_type="Release")
        result = inst.prompt_validated_option(
            "build_type", "Build type",
            ["Debug", "Release", "RelWithDebInfo", "MinSizeRel", "DebugFast"],
            normalizer=lambda v: v.lower().replace("-", ""),
        )
        self.assertEqual(result, "Release")

    def test_invalid_raises_after_retries(self):
        inst = self._make(build_type="InvalidType")
        with self.assertRaises(ValueError):
            inst.prompt_validated_option(
                "build_type", "Build type",
                ["Debug", "Release"],
                normalizer=lambda v: v.lower(),
            )

    def test_allow_empty_with_none(self):
        inst = self._make(sanitizer="none")
        result = inst.prompt_validated_option(
            "sanitizer", "Sanitizer",
            ["ASan", "UBSan"],
            allow_empty=True,
        )
        self.assertEqual(result, "")


class TestPromptBuildTypeOption(unittest.TestCase):
    """Tests for prompt_build_type_option non-interactive path."""

    def test_valid_build_type(self):
        args = {"non_interactive": True, "plain_ui": True, "build_type": "Debug"}
        with tempfile.TemporaryDirectory() as tmp:
            inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
        result = inst.prompt_build_type_option("build_type")
        self.assertEqual(result, "Debug")

    def test_relwithdebinfo(self):
        args = {"non_interactive": True, "plain_ui": True, "build_type": "RelWithDebInfo"}
        with tempfile.TemporaryDirectory() as tmp:
            inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
        result = inst.prompt_build_type_option("build_type")
        self.assertEqual(result, "RelWithDebInfo")


class TestPromptSanitizerOption(unittest.TestCase):
    """Tests for prompt_sanitizer_option non-interactive path."""

    def test_valid_sanitizer(self):
        args = {"non_interactive": True, "plain_ui": True, "sanitizer": "ASan"}
        with tempfile.TemporaryDirectory() as tmp:
            inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
        result = inst.prompt_sanitizer_option("sanitizer")
        self.assertEqual(result, "ASan")

    def test_empty_sanitizer(self):
        args = {"non_interactive": True, "plain_ui": True, "sanitizer": "none"}
        with tempfile.TemporaryDirectory() as tmp:
            inst = MrDocsInstaller(cmd_line_args=args, source_dir=tmp)
        result = inst.prompt_sanitizer_option("sanitizer")
        self.assertEqual(result, "")


class TestWriteEnvFile(unittest.TestCase):
    """Tests for write_env_file using a temp directory."""

    def test_no_env_file_returns_early(self):
        inst = _make_installer()
        inst.options.env_file = ""
        inst.write_env_file()  # Should not raise

    def test_writes_package_roots(self):
        with tempfile.TemporaryDirectory() as tmp:
            env_path = os.path.join(tmp, "env.txt")
            inst = _make_installer()
            inst.options.env_file = env_path
            inst.options.dry_run = False
            inst.package_roots = {
                "LLVM_ROOT": "/opt/llvm",
                "Lua_ROOT": "/opt/lua",
            }
            inst.write_env_file()
            with open(env_path) as f:
                content = f.read()
            self.assertIn("LLVM_ROOT=/opt/llvm", content)
            self.assertIn("Lua_ROOT=/opt/lua", content)

    def test_writes_libcxx_flags(self):
        with tempfile.TemporaryDirectory() as tmp:
            env_path = os.path.join(tmp, "env.txt")
            inst = _make_installer()
            inst.options.env_file = env_path
            inst.options.dry_run = False
            inst._libcxx_cxxflags = "-isystem /opt/libcxx/include"
            inst._libcxx_ldflags = "-L/opt/libcxx/lib"
            inst.write_env_file()
            with open(env_path) as f:
                content = f.read()
            self.assertIn("BOOTSTRAP_CXXFLAGS=-isystem /opt/libcxx/include", content)
            self.assertIn("BOOTSTRAP_LDFLAGS=-L/opt/libcxx/lib", content)

    def test_writes_sanitizer_flag(self):
        with tempfile.TemporaryDirectory() as tmp:
            env_path = os.path.join(tmp, "env.txt")
            inst = _make_installer()
            inst.options.env_file = env_path
            inst.options.dry_run = False
            inst.options.sanitizer = "ASan"
            inst.write_env_file()
            with open(env_path) as f:
                content = f.read()
            self.assertIn("BOOTSTRAP_LDFLAGS=-fsanitize=address", content)

    def test_dry_run_prints_to_stdout(self):
        inst = _make_installer()
        inst.options.env_file = "/tmp/test_env.txt"
        inst.options.dry_run = True
        inst.package_roots = {"LLVM_ROOT": "/opt/llvm"}
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst.write_env_file()
        output = buf.getvalue()
        self.assertIn("LLVM_ROOT=/opt/llvm", output)

    def test_creates_parent_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            env_path = os.path.join(tmp, "subdir", "env.txt")
            inst = _make_installer()
            inst.options.env_file = env_path
            inst.options.dry_run = False
            inst.package_roots = {"FOO": "bar"}
            inst.write_env_file()
            self.assertTrue(os.path.exists(env_path))


class TestGetCacheKey(unittest.TestCase):
    """Tests for get_cache_key with mocked recipe loading."""

    @patch("src.installer.generate_cache_key", return_value="llvm-abc123-Release-ubuntu-gcc-15")
    @patch("src.installer.detect_compiler_for_cache_key", return_value=("gcc", "15"))
    @patch("src.installer.load_recipe_files")
    def test_returns_cache_key(self, mock_load, mock_detect, mock_generate):
        mock_recipe = MagicMock()
        mock_recipe.name = "llvm"
        mock_recipe.version = "19.1"
        mock_recipe.source.commit = "abc123"
        mock_load.return_value = [mock_recipe]

        inst = _make_installer(cc="/usr/bin/gcc-15")
        inst.options.cc = "/usr/bin/gcc-15"
        inst.options.os_key = "ubuntu:24.04"
        inst.options.build_type = "Release"
        inst.options.preset = "default"

        result = inst.get_cache_key("llvm")
        self.assertEqual(result, "llvm-abc123-Release-ubuntu-gcc-15")
        mock_generate.assert_called_once()

    @patch("src.installer.load_recipe_files")
    def test_unknown_recipe_raises(self, mock_load):
        mock_recipe = MagicMock()
        mock_recipe.name = "llvm"
        mock_load.return_value = [mock_recipe]

        inst = _make_installer()
        inst.options.os_key = "ubuntu:24.04"
        with self.assertRaises(RuntimeError):
            inst.get_cache_key("nonexistent")

    @patch("src.installer.load_recipe_files")
    def test_missing_os_key_raises(self, mock_load):
        mock_recipe = MagicMock()
        mock_recipe.name = "llvm"
        mock_recipe.source.commit = "abc"
        mock_load.return_value = [mock_recipe]

        inst = _make_installer()
        inst.options.os_key = ""
        inst.options.cc = "/usr/bin/gcc"
        with self.assertRaises(RuntimeError):
            inst.get_cache_key("llvm")


class TestListRecipes(unittest.TestCase):
    """Tests for list_recipes with mocked recipe directory."""

    @patch("src.installer.load_recipe_files")
    def test_prints_recipes(self, mock_load):
        r1 = MagicMock()
        r1.name = "llvm"
        r1.version = "19.1"
        r1.dependencies = []
        r2 = MagicMock()
        r2.name = "lua"
        r2.version = "5.4"
        r2.dependencies = ["llvm"]
        mock_load.return_value = [r1, r2]

        inst = _make_installer()
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst.list_recipes()
        output = buf.getvalue()
        self.assertIn("llvm", output)
        self.assertIn("lua", output)
        self.assertIn("Dependencies: llvm", output)

    @patch("src.installer.load_recipe_files")
    def test_no_recipes(self, mock_load):
        mock_load.return_value = []
        inst = _make_installer()
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst.list_recipes()
        self.assertIn("No recipes found", buf.getvalue())


###############################################################################
# US-008 – I/O methods with mocking
###############################################################################

class TestCheckTool(unittest.TestCase):
    """Tests for check_tool with mocked find_tool and is_tool_executable."""

    @patch("src.installer.is_tool_executable", return_value=True)
    @patch("src.installer.find_tool", return_value="/usr/bin/cmake")
    def test_check_tool_found_and_executable(self, mock_find, mock_exec):
        inst = _make_installer()
        result = inst.check_tool("cmake")
        self.assertEqual(result, "/usr/bin/cmake")

    @patch("src.installer.is_tool_executable", return_value=False)
    @patch("src.installer.find_tool", return_value="/usr/bin/cmake")
    def test_check_tool_not_executable_raises(self, mock_find, mock_exec):
        inst = _make_installer()
        with self.assertRaises(FileNotFoundError):
            inst.check_tool("cmake")

    @patch("src.installer.find_tool", return_value="/usr/bin/git")
    def test_check_tool_dry_run_skips_validation(self, mock_find):
        inst = _make_installer(dry_run=True)
        result = inst.check_tool("git")
        self.assertEqual(result, "/usr/bin/git")

    @patch("src.installer.is_tool_executable", side_effect=[False, True])
    @patch("src.installer.find_tool", return_value="/usr/bin/cmake")
    def test_check_tool_retries_on_invalid(self, mock_find, mock_exec):
        """First attempt fails, second succeeds after re-prompt."""
        inst = _make_installer()
        result = inst.check_tool("cmake")
        self.assertEqual(result, "/usr/bin/cmake")
        self.assertEqual(mock_exec.call_count, 2)


class TestSetupCompilers(unittest.TestCase):
    """Tests for setup_compilers with mocked prompt methods."""

    @patch("src.installer.is_tool_executable", return_value=True)
    def test_setup_compilers_non_interactive(self, mock_exec):
        inst = _make_installer(cc="/usr/bin/gcc", cxx="/usr/bin/g++")
        inst.setup_compilers()
        self.assertEqual(inst.options.cc, "/usr/bin/gcc")
        self.assertEqual(inst.options.cxx, "/usr/bin/g++")

    def test_setup_compilers_empty_allowed(self):
        inst = _make_installer(cc="", cxx="")
        inst.setup_compilers()
        self.assertEqual(inst.options.cc, "")
        self.assertEqual(inst.options.cxx, "")

    def test_setup_compilers_dry_run_skips_validation(self):
        inst = _make_installer(cc="/usr/bin/gcc", cxx="/usr/bin/g++", dry_run=True)
        inst.setup_compilers()
        self.assertEqual(inst.options.cc, "/usr/bin/gcc")
        self.assertEqual(inst.options.cxx, "/usr/bin/g++")


class TestSetupBuildOptions(unittest.TestCase):
    """Tests for setup_build_options with mocked prompt methods."""

    @patch("src.installer.is_tool_executable", return_value=True)
    @patch("src.installer.find_tool", return_value="/usr/bin/java")
    def test_setup_build_options_with_tests(self, mock_find, mock_exec):
        inst = _make_installer(build_type="Release", sanitizer="none", build_tests=True)
        inst.setup_build_options()
        self.assertEqual(inst.options.build_type, "Release")
        self.assertEqual(inst.options.sanitizer, "")

    def test_setup_build_options_no_tests(self):
        inst = _make_installer(build_type="Debug", sanitizer="none", build_tests=False)
        inst.setup_build_options()
        self.assertEqual(inst.options.build_type, "Debug")
        self.assertFalse(inst.options.build_tests)


class TestInstallDependencies(unittest.TestCase):
    """Tests for install_dependencies with mocked recipe functions."""

    def _make_recipe(self, name="llvm", version="19.1"):
        r = MagicMock()
        r.name = name
        r.version = version
        r.dependencies = []
        r.package_root_var = f"{name}_ROOT"
        r.install_dir = f"/opt/{name}"
        r.source_dir = f"/tmp/{name}-src"
        r.build_dir = f"/tmp/{name}-build"
        r.source.commit = "abc123"
        r.source.tag = ""
        r.source.branch = ""
        r.source.ref = ""
        r.build = []
        return r

    @patch("src.installer.write_recipe_stamp")
    @patch("src.installer.build_recipe")
    @patch("src.installer.apply_recipe_patches")
    @patch("src.installer.fetch_recipe_source")
    @patch("src.installer.topo_sort_recipes", side_effect=lambda x: x)
    @patch("src.installer.load_recipe_files")
    def test_install_dependencies_builds_recipes(
        self, mock_load, mock_topo, mock_fetch, mock_patch, mock_build, mock_stamp
    ):
        recipe = self._make_recipe()
        mock_load.return_value = [recipe]

        inst = _make_installer()
        inst.install_dependencies()

        mock_fetch.assert_called_once()
        mock_build.assert_called_once()
        mock_stamp.assert_called_once()
        self.assertIn("llvm_ROOT", inst.package_roots)

    @patch("src.installer.load_recipe_files")
    def test_install_dependencies_no_recipes_raises(self, mock_load):
        mock_load.return_value = []
        inst = _make_installer()
        with self.assertRaises(RuntimeError):
            inst.install_dependencies()

    @patch("src.installer.is_recipe_up_to_date", return_value="")
    @patch("src.installer.topo_sort_recipes", side_effect=lambda x: x)
    @patch("src.installer.load_recipe_files")
    def test_install_dependencies_skips_up_to_date(self, mock_load, mock_topo, mock_uptodate):
        recipe = self._make_recipe()
        mock_load.return_value = [recipe]

        inst = _make_installer()
        inst.install_dependencies()

        self.assertIn("llvm_ROOT", inst.package_roots)

    @patch("src.installer.write_recipe_stamp")
    @patch("src.installer.build_recipe")
    @patch("src.installer.apply_recipe_patches")
    @patch("src.installer.fetch_recipe_source")
    @patch("src.installer.topo_sort_recipes", side_effect=lambda x: x)
    @patch("src.installer.load_recipe_files")
    def test_install_dependencies_with_recipe_filter(
        self, mock_load, mock_topo, mock_fetch, mock_patch, mock_build, mock_stamp
    ):
        r1 = self._make_recipe("llvm")
        r2 = self._make_recipe("lua", "5.4")
        mock_load.return_value = [r1, r2]

        inst = _make_installer(recipe_filter="lua")
        inst.install_dependencies()

        # Only lua should be built
        self.assertEqual(mock_build.call_count, 1)

    @patch("src.installer.write_recipe_stamp")
    @patch("src.installer.build_recipe")
    @patch("src.installer.apply_recipe_patches")
    @patch("src.installer.fetch_recipe_source")
    @patch("src.installer.topo_sort_recipes", side_effect=lambda x: x)
    @patch("src.installer.load_recipe_files")
    def test_install_dependencies_cache_dir_overrides_install_dir(
        self, mock_load, mock_topo, mock_fetch, mock_patch, mock_build, mock_stamp
    ):
        recipe = self._make_recipe()
        mock_load.return_value = [recipe]

        inst = _make_installer(cache_dir="/cache")
        inst.install_dependencies()

        self.assertEqual(recipe.install_dir, "/cache/llvm")


class TestCreatePresets(unittest.TestCase):
    """Tests for create_presets with mocked create_cmake_presets."""

    @patch("src.installer.create_cmake_presets")
    def test_create_presets_calls_cmake_presets(self, mock_create):
        inst = _make_installer(build_type="Release")
        inst.options.preset = "release-linux-gcc"
        inst.create_presets()
        mock_create.assert_called_once()
        # Verify key arguments
        call_kwargs = mock_create.call_args
        self.assertEqual(call_kwargs[0][1], "release-linux-gcc")  # preset name
        self.assertEqual(call_kwargs[0][2], "Release")  # build type

    @patch("src.installer.create_cmake_presets")
    def test_create_presets_passes_package_roots(self, mock_create):
        inst = _make_installer()
        inst.options.preset = "test"
        inst.package_roots = {"LLVM_ROOT": "/opt/llvm"}
        inst.create_presets()
        call_args = mock_create.call_args
        self.assertEqual(call_args[0][9], {"LLVM_ROOT": "/opt/llvm"})


class TestInstallMrdocs(unittest.TestCase):
    """Tests for install_mrdocs with mocked cmake_workflow."""

    @patch.object(MrDocsInstaller, "print_mrdocs_summary")
    @patch.object(MrDocsInstaller, "cmake_workflow")
    @patch("src.installer.check_git_symlinks")
    def test_install_mrdocs_calls_cmake_workflow(self, mock_git, mock_workflow, mock_summary):
        inst = _make_installer(build_type="Release")
        inst.options.preset = "release-linux"
        inst.install_mrdocs()
        mock_workflow.assert_called_once()
        mock_git.assert_called_once()

    def test_install_mrdocs_skip_build(self):
        inst = _make_installer(skip_build=True)
        # Should not raise or call cmake_workflow
        inst.install_mrdocs()

    @patch.object(MrDocsInstaller, "print_mrdocs_summary")
    @patch.object(MrDocsInstaller, "cmake_workflow")
    @patch("src.installer.check_git_symlinks")
    def test_install_mrdocs_debugfast_maps_to_debug(self, mock_git, mock_workflow, mock_summary):
        inst = _make_installer(build_type="DebugFast")
        inst.options.preset = "debug-linux"
        inst.install_mrdocs()
        # cmake_workflow's second arg (build_type) should be "Debug"
        call_args = mock_workflow.call_args[0]
        self.assertEqual(call_args[1], "Debug")


class TestDryRunOutput(unittest.TestCase):
    """Tests for _dry_comment, _dry_preamble, _dry_config_summary capturing stdout."""

    def test_dry_comment_prints_in_dry_run(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True)
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_comment("hello world")
        self.assertIn("# hello world", buf.getvalue())

    def test_dry_comment_silent_when_not_dry_run(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer()
        inst.options.dry_run = False
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_comment("hello world")
        self.assertEqual(buf.getvalue(), "")

    def test_dry_preamble_prints_shebang(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True)
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_preamble()
        output = buf.getvalue()
        self.assertIn("#!/usr/bin/env bash", output)
        self.assertIn("set -euo pipefail", output)
        self.assertIn("MrDocs bootstrap", output)

    def test_dry_preamble_silent_when_not_dry_run(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer()
        inst.options.dry_run = False
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_preamble()
        self.assertEqual(buf.getvalue(), "")

    def test_dry_config_summary_exports_cc(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True, cc="/usr/bin/gcc-15")
        inst.options.cc = "/usr/bin/gcc-15"
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_config_summary()
        output = buf.getvalue()
        self.assertIn("export CC=", output)
        self.assertIn("/usr/bin/gcc-15", output)
        self.assertIn("export BUILD_TYPE=", output)

    def test_dry_config_summary_exports_sanitizer(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True, sanitizer="ASan")
        inst.options.sanitizer = "ASan"
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_config_summary()
        output = buf.getvalue()
        self.assertIn("export SANITIZER=", output)

    def test_dry_config_summary_exports_compiler_flags(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True)
        inst.options.cflags = "-O2"
        inst.options.cxxflags = "-std=c++20"
        inst.options.ldflags = "-lm"
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_config_summary()
        output = buf.getvalue()
        self.assertIn("export CFLAGS=", output)
        self.assertIn("export CXXFLAGS=", output)
        self.assertIn("export LDFLAGS=", output)

    def test_dry_config_summary_silent_when_not_dry_run(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer()
        inst.options.dry_run = False
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_config_summary()
        self.assertEqual(buf.getvalue(), "")

    def test_dry_config_summary_exports_tool_paths(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True)
        inst.options.cmake_path = "/usr/bin/cmake"
        inst.options.ninja_path = "/usr/bin/ninja"
        inst.options.git_path = "/usr/bin/git"
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst._dry_config_summary()
        output = buf.getvalue()
        self.assertIn("export CMAKE=", output)
        self.assertIn("export NINJA=", output)
        self.assertIn("export GIT=", output)


class TestGenerateConfigs(unittest.TestCase):
    """Tests for generate_configs dispatching."""

    @patch("src.installer.generate_pretty_printer_configs")
    @patch("src.installer.generate_run_configs")
    def test_generate_configs_calls_both(self, mock_run_configs, mock_pp_configs):
        inst = _make_installer()
        inst.options.generate_run_configs = True
        inst.options.generate_pretty_printer_configs = True
        inst.generate_configs()
        mock_run_configs.assert_called_once()
        mock_pp_configs.assert_called_once()

    @patch("src.installer.generate_run_configs")
    def test_generate_configs_skipped_when_disabled(self, mock_run_configs):
        inst = _make_installer()
        inst.options.generate_run_configs = False
        inst.generate_configs()
        mock_run_configs.assert_not_called()


class TestSetupNinja(unittest.TestCase):
    """Tests for setup_ninja with mocked install_ninja."""

    @patch("src.installer.install_ninja", return_value="/usr/local/bin/ninja")
    def test_setup_ninja_updates_path(self, mock_install):
        inst = _make_installer()
        inst.setup_ninja()
        self.assertEqual(inst.options.ninja_path, "/usr/local/bin/ninja")

    @patch("src.installer.install_ninja", return_value=None)
    def test_setup_ninja_no_path_returned(self, mock_install):
        inst = _make_installer()
        original = inst.options.ninja_path
        inst.setup_ninja()
        self.assertEqual(inst.options.ninja_path, original)


class TestCheckSystemPrerequisites(unittest.TestCase):
    """Tests for check_system_prerequisites."""

    def test_dry_run_prints_prerequisites(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer(dry_run=True)
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst.check_system_prerequisites()
        output = buf.getvalue()
        self.assertIn("cmake", output)
        self.assertIn("git", output)

    @patch("src.installer.check_prerequisites", return_value=[])
    def test_all_prerequisites_found(self, mock_check):
        inst = _make_installer()
        inst.check_system_prerequisites()  # Should not raise

    @patch("src.installer.report_missing_prerequisites")
    @patch("src.installer.check_prerequisites")
    def test_missing_required_prerequisite_raises(self, mock_check, mock_report):
        missing_item = MagicMock()
        missing_item.required = True
        mock_check.return_value = [missing_item]

        inst = _make_installer()
        with self.assertRaises(RuntimeError):
            inst.check_system_prerequisites()


class TestPrintRecipeSummary(unittest.TestCase):
    """Tests for print_recipe_summary output."""

    def test_prints_source_build_install(self):
        import io
        from contextlib import redirect_stdout
        inst = _make_installer()
        recipe = MagicMock()
        recipe.source_dir = "/tmp/src"
        recipe.build_dir = "/tmp/build"
        recipe.install_dir = "/tmp/install"
        buf = io.StringIO()
        with redirect_stdout(buf):
            inst.print_recipe_summary(recipe)
        output = buf.getvalue()
        self.assertIn("Source", output)
        self.assertIn("Build", output)
        self.assertIn("Install", output)


if __name__ == "__main__":
    unittest.main()
