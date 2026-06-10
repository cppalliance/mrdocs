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

"""Tests for run_configs.py orchestration and helper functions."""

import os
import sys
import unittest
from unittest.mock import patch, MagicMock, call

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.configs.run_configs import (
    expand_with,
    format_values,
    get_dynamic_run_configs,
    generate_run_configs,
)
from src.core.options import InstallOptions
from src.core.ui import TextUI


def _make_options(**overrides):
    """Create InstallOptions with sensible test defaults."""
    defaults = dict(
        source_dir="/src",
        build_dir="/src/build/release",
        install_dir="/src/install/release",
        build_type="Release",
        preset="release-linux-gcc",
        non_interactive=True,
        cc="/usr/bin/gcc",
        cxx="/usr/bin/g++",
        boost_src_dir="",
        java_path="",
        ninja_path="",
        build_tests=True,
        jetbrains_run_config_dir="",
    )
    defaults.update(overrides)
    return InstallOptions(**defaults)


# ── expand_with ──────────────────────────────────────────────────────

class TestExpandWith(unittest.TestCase):
    def test_simple_dollar_var(self):
        self.assertEqual(expand_with("$foo", {"foo": "bar"}), "bar")

    def test_braced_var(self):
        self.assertEqual(expand_with("${foo}", {"foo": "bar"}), "bar")

    def test_missing_var_unchanged(self):
        self.assertEqual(expand_with("$missing", {}), "$missing")

    def test_multiple_vars(self):
        result = expand_with("$a/$b", {"a": "x", "b": "y"})
        self.assertEqual(result, "x/y")

    def test_no_placeholders(self):
        self.assertEqual(expand_with("hello", {}), "hello")

    def test_empty_string(self):
        self.assertEqual(expand_with("", {}), "")

    def test_non_string_value(self):
        self.assertEqual(expand_with("cores=$n", {"n": 4}), "cores=4")


# ── format_values ────────────────────────────────────────────────────

class TestFormatValues(unittest.TestCase):
    def test_string(self):
        self.assertEqual(format_values("$x", {"x": "1"}), "1")

    def test_list(self):
        result = format_values(["$a", "$b"], {"a": "1", "b": "2"})
        self.assertEqual(result, ["1", "2"])

    def test_dict(self):
        result = format_values({"k": "$v"}, {"v": "val"})
        self.assertEqual(result, {"k": "val"})

    def test_nested(self):
        obj = {"items": ["$x", {"nested": "$y"}]}
        result = format_values(obj, {"x": "A", "y": "B"})
        self.assertEqual(result, {"items": ["A", {"nested": "B"}]})

    def test_non_string_passthrough(self):
        self.assertEqual(format_values(42, {}), 42)
        self.assertIsNone(format_values(None, {}))
        self.assertTrue(format_values(True, {}))


# ── get_dynamic_run_configs ──────────────────────────────────────────

class TestGetDynamicRunConfigs(unittest.TestCase):
    def test_basic_bootstrap_configs(self):
        """Always produces at least the bootstrap helper configs."""
        opts = _make_options()
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults)
        names = [c["name"] for c in configs]
        self.assertTrue(any("Bootstrap Help" in n for n in names))
        self.assertTrue(any("Bootstrap Update" in n for n in names))
        self.assertTrue(any("Bootstrap Refresh" in n for n in names))
        self.assertTrue(any("Reformat" in n for n in names))

    def test_preset_in_config_name(self):
        opts = _make_options(preset="debug-linux-gcc")
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults)
        update_names = [c["name"] for c in configs if "Update" in c["name"]]
        self.assertTrue(any("debug-linux-gcc" in n for n in update_names))

    def test_no_boost_configs_without_boost_src(self):
        opts = _make_options(boost_src_dir="")
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults)
        boost = [c for c in configs if "Boost" in c.get("name", "")]
        self.assertEqual(len(boost), 0)

    @patch("src.configs.run_configs.os.path.exists", return_value=True)
    @patch("src.configs.run_configs.os.listdir", return_value=["url"])
    @patch("src.configs.run_configs.os.cpu_count", return_value=8)
    def test_boost_configs_with_valid_boost_dir(self, mock_cpu, mock_ls, mock_exists):
        opts = _make_options(boost_src_dir="/boost")
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults)
        boost = [c for c in configs if "Boost" in c.get("name", "")]
        # os.path.exists returns True for the mrdocs.yml check
        self.assertGreater(len(boost), 0)
        self.assertIn("Boost.Url Documentation", boost[0]["name"])

    def test_java_config_added(self):
        opts = _make_options()
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults, java_path="/usr/bin/java")
        java_configs = [c for c in configs if "RelaxNG" in c.get("name", "")]
        self.assertEqual(len(java_configs), 1)

    @patch("src.configs.run_configs.is_windows", return_value=False)
    def test_java_and_libxml2_unix(self, _mock_win):
        opts = _make_options()
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults, java_path="/usr/bin/java", libxml2_root="/opt/libxml2")
        lint_configs = [c for c in configs if "XML Lint" in c.get("name", "")]
        self.assertEqual(len(lint_configs), 1)
        # Unix uses find command
        self.assertEqual(lint_configs[0]["script"], "find")

    @patch("src.configs.run_configs.is_windows", return_value=True)
    @patch("src.configs.run_configs.os.walk", return_value=[
        ("/golden", [], ["a.xml", "b.bad.xml", "c.xml"])
    ])
    def test_java_and_libxml2_windows(self, _mock_walk, _mock_win):
        opts = _make_options()
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults, java_path="/usr/bin/java", libxml2_root="/opt/libxml2")
        lint_configs = [c for c in configs if "XML Lint" in c.get("name", "")]
        self.assertEqual(len(lint_configs), 1)
        # Windows enumerates XML files directly; bad.xml excluded
        args = lint_configs[0]["args"]
        xml_files = [a for a in args if a.endswith(".xml") and "rng" not in a]
        self.assertEqual(len(xml_files), 2)  # a.xml and c.xml, not b.bad.xml

    def test_no_libxml2_without_java(self):
        opts = _make_options()
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults, java_path="", libxml2_root="/opt/libxml2")
        lint = [c for c in configs if "XML Lint" in c.get("name", "")]
        self.assertEqual(len(lint), 0)

    def test_bootstrap_args_include_non_default_options(self):
        """Options differing from defaults appear in bootstrap args."""
        opts = _make_options(build_type="Debug", cc="/usr/bin/gcc")
        defaults = _make_options(build_type="Release", cc="")
        configs = get_dynamic_run_configs(opts, defaults)
        update_cfg = [c for c in configs if "Update" in c["name"] and "Refresh" not in c["name"]][0]
        self.assertIn("--build-type", update_cfg["args"])
        self.assertIn("Debug", update_cfg["args"])

    def test_bootstrap_args_bool_false(self):
        """Boolean options that are False emit --no-<flag>."""
        opts = _make_options(build_tests=False)
        defaults = _make_options(build_tests=True)
        configs = get_dynamic_run_configs(opts, defaults)
        update_cfg = [c for c in configs if "Update" in c["name"] and "Refresh" not in c["name"]][0]
        self.assertIn("--no-build-tests", update_cfg["args"])

    def test_non_interactive_skipped_in_args(self):
        """The non_interactive field is always skipped in bootstrap args."""
        opts = _make_options(non_interactive=True)
        defaults = _make_options(non_interactive=False)
        configs = get_dynamic_run_configs(opts, defaults)
        update_cfg = [c for c in configs if "Update" in c["name"] and "Refresh" not in c["name"]][0]
        self.assertNotIn("--non-interactive", update_cfg["args"])

    def test_refresh_config_has_non_interactive_flag(self):
        """Refresh config always has --non-interactive in its args."""
        opts = _make_options()
        defaults = _make_options()
        configs = get_dynamic_run_configs(opts, defaults)
        refresh = [c for c in configs if "Bootstrap Refresh (" in c["name"]][0]
        self.assertIn("--non-interactive", refresh["args"])


# ── generate_run_configs ─────────────────────────────────────────────

class TestGenerateRunConfigs(unittest.TestCase):
    def setUp(self):
        self.ui = TextUI()
        self.opts = _make_options()
        self.defaults = _make_options()

    @patch("src.configs.run_configs.load_json_file")
    def test_raises_on_empty_configs(self, mock_load):
        mock_load.return_value = {"configs": []}
        with self.assertRaises(RuntimeError):
            generate_run_configs(self.opts, self.defaults, ui=self.ui)

    @patch("src.configs.run_configs.load_json_file")
    def test_raises_on_none_configs(self, mock_load):
        mock_load.return_value = {}
        with self.assertRaises(RuntimeError):
            generate_run_configs(self.opts, self.defaults, ui=self.ui)

    @patch("src.configs.run_configs.load_json_file")
    def test_raises_on_null_return(self, mock_load):
        mock_load.return_value = None
        with self.assertRaises(RuntimeError):
            generate_run_configs(self.opts, self.defaults, ui=self.ui)

    def _base_json(self, **overrides):
        """Minimal valid run_configs.json content."""
        data = {
            "configs": [
                {"name": "Test Config", "script": "$source_dir/test.py", "args": []}
            ],
        }
        data.update(overrides)
        return data

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_dispatches_to_clion(self, mock_load, _mock_dyn):
        mock_load.return_value = self._base_json()
        mock_clion_fn = MagicMock()
        mock_mod = MagicMock()
        mock_mod.generate_clion_run_configs = mock_clion_fn
        # Lazy import resolves to src.configs.clion; pre-seed sys.modules
        with patch.dict("sys.modules", {"src.configs.clion": mock_mod}):
            generate_run_configs(
                self.opts, self.defaults,
                generate_clion=True, generate_vscode=False, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
            mock_clion_fn.assert_called_once()

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_dispatches_to_vscode(self, mock_load, _mock_dyn):
        mock_load.return_value = self._base_json()
        mock_vscode_fn = MagicMock()
        mock_mod = MagicMock()
        mock_mod.generate_vscode_run_configs = mock_vscode_fn
        with patch.dict("sys.modules", {"src.configs.vscode": mock_mod}):
            generate_run_configs(
                self.opts, self.defaults,
                generate_clion=False, generate_vscode=True, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
            mock_vscode_fn.assert_called_once()

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_dispatches_to_vs(self, mock_load, _mock_dyn):
        mock_load.return_value = self._base_json()
        mock_vs_fn = MagicMock()
        mock_mod = MagicMock()
        mock_mod.generate_visual_studio_run_configs = mock_vs_fn
        with patch.dict("sys.modules", {"src.configs.visual_studio": mock_mod}):
            generate_run_configs(
                self.opts, self.defaults,
                generate_clion=False, generate_vscode=False, generate_vs=True,
                generate_justfile=False,
                ui=self.ui,
            )
            mock_vs_fn.assert_called_once()

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_dispatches_to_justfile(self, mock_load, _mock_dyn):
        mock_load.return_value = self._base_json()
        mock_fn = MagicMock()
        mock_mod = MagicMock()
        mock_mod.generate_justfile_run_configs = mock_fn
        with patch.dict("sys.modules", {"src.configs.justfile": mock_mod}):
            generate_run_configs(
                self.opts, self.defaults,
                generate_clion=False, generate_vscode=False, generate_vs=False,
                generate_justfile=True,
                ui=self.ui,
            )
            mock_fn.assert_called_once()

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_skips_ide_when_generate_flag_false(self, mock_load, _mock_dyn):
        mock_load.return_value = self._base_json()
        # All generate flags False — no generators called
        generate_run_configs(
            self.opts, self.defaults,
            generate_clion=False, generate_vscode=False, generate_vs=False,
            generate_justfile=False,
            ui=self.ui,
        )

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_token_replacement_applied(self, mock_load, _mock_dyn):
        """Config strings have $source_dir replaced."""
        mock_load.return_value = self._base_json()
        captured = {}
        def fake_clion(configs, **kw):
            captured["configs"] = configs
        mock_mod = MagicMock()
        mock_mod.generate_clion_run_configs = fake_clion
        with patch.dict("sys.modules", {"src.configs.clion": mock_mod}):
            generate_run_configs(
                self.opts, self.defaults,
                generate_clion=True, generate_vscode=False, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
        self.assertIn("/src/test.py", captured["configs"][0]["script"])

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_mrdocs_prefixed_tokens_expand(self, mock_load, _mock_dyn):
        """The mrdocs_* placeholders used in run_configs.json expand
        to real paths rather than passing through literally."""
        data = self._base_json()
        data["configs"] = [{
            "name": "Tokened",
            "target": "mrdocs",
            "program": "${mrdocs_build_dir}/mrdocs",
            "args": [
                "${mrdocs_src_dir}/test-files",
                "--output=${mrdocs_install_dir}/out",
            ],
        }]
        mock_load.return_value = data
        captured = {}
        def fake_clion(configs, **kw):
            captured["configs"] = configs
        mock_mod = MagicMock()
        mock_mod.generate_clion_run_configs = fake_clion
        with patch.dict("sys.modules", {"src.configs.clion": mock_mod}):
            generate_run_configs(
                self.opts, self.defaults,
                generate_clion=True, generate_vscode=False, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
        cfg = captured["configs"][0]
        self.assertEqual(cfg["program"], "/src/build/release/mrdocs")
        self.assertEqual(cfg["args"][0], "/src/test-files")
        self.assertEqual(cfg["args"][1], "--output=/src/install/release/out")
        for value in [cfg["program"], *cfg["args"]]:
            self.assertNotIn("${mrdocs_", value)

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_filters_configs_requiring_java(self, mock_load, _mock_dyn):
        data = self._base_json()
        data["configs"].append({"name": "Java Thing", "requires": ["java"], "script": "j"})
        mock_load.return_value = data
        captured = {}
        def fake_clion(configs, **kw):
            captured["configs"] = configs
        mock_mod = MagicMock()
        mock_mod.generate_clion_run_configs = fake_clion
        with patch.dict("sys.modules", {"src.configs.clion": mock_mod}):
            opts = _make_options(java_path="")
            generate_run_configs(
                opts, self.defaults,
                generate_clion=True, generate_vscode=False, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
        names = [c["name"] for c in captured["configs"]]
        self.assertNotIn("Java Thing", names)

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_filters_configs_requiring_build_tests(self, mock_load, _mock_dyn):
        data = self._base_json()
        data["configs"].append({"name": "Test Runner", "requires": ["build_tests"], "script": "t"})
        mock_load.return_value = data
        captured = {}
        def fake_clion(configs, **kw):
            captured["configs"] = configs
        mock_mod = MagicMock()
        mock_mod.generate_clion_run_configs = fake_clion
        with patch.dict("sys.modules", {"src.configs.clion": mock_mod}):
            opts = _make_options(build_tests=False)
            generate_run_configs(
                opts, self.defaults,
                generate_clion=True, generate_vscode=False, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
        names = [c["name"] for c in captured["configs"]]
        self.assertNotIn("Test Runner", names)

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_libxml2_extracted_from_package_roots(self, mock_load, _mock_dyn):
        mock_load.return_value = self._base_json()
        pkg_roots = {"LibXml2": "/opt/libxml2"}
        with patch("src.configs.run_configs.get_dynamic_run_configs") as mock_dyn:
            mock_dyn.return_value = []
            generate_run_configs(
                self.opts, self.defaults,
                package_roots=pkg_roots,
                generate_clion=False, generate_vscode=False, generate_vs=False,
                generate_justfile=False,
                ui=self.ui,
            )
            # get_dynamic_run_configs should receive libxml2_root
            _, kwargs = mock_dyn.call_args
            self.assertEqual(kwargs.get("libxml2_root"), "/opt/libxml2")

    @patch("src.configs.run_configs.get_dynamic_run_configs", return_value=[])
    @patch("src.configs.run_configs.load_json_file")
    def test_defaults_none_params(self, mock_load, _mock_dyn):
        """package_roots, compiler_info default to empty dicts when None."""
        mock_load.return_value = self._base_json()
        # Should not raise
        generate_run_configs(
            self.opts, self.defaults,
            package_roots=None, compiler_info=None,
            generate_clion=False, generate_vscode=False, generate_vs=False,
            generate_justfile=False,
            ui=self.ui,
        )


if __name__ == "__main__":
    unittest.main()
