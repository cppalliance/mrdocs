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

"""Tests for the Justfile generator (justfile.py)."""

import os
import re
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.configs.justfile import (
    slugify_recipe_id,
    slugify_token,
    recipe_id_for,
    assign_recipe_ids,
    generate_justfile_run_configs,
    _parse_segments,
    _is_stale_variant,
)
from src.core.ui import TextUI

_VALID = re.compile(r"^[a-zA-Z_][a-zA-Z0-9_-]*$")


def _gen(configs, preset="release-macos", all_presets=None,
         default_preset="release-macos", existing=None, group_order=None):
    """Generate a justfile in a temp dir and return its contents."""
    d = tempfile.mkdtemp()
    jf = os.path.join(d, "justfile")
    if existing is not None:
        with open(jf, "w", encoding="utf-8") as f:
            f.write(existing)
    generate_justfile_run_configs(
        configs,
        source_dir=d,
        build_dir=os.path.join(d, "build", "release-macos"),
        preset=preset,
        dry_run=False,
        ui=TextUI(),
        all_presets=all_presets if all_presets is not None else ["release-macos"],
        default_preset=default_preset,
        group_order=group_order,
        justfile_path=jf,
    )
    with open(jf, encoding="utf-8") as f:
        return f.read()


def _recipe_block(content, rid):
    """Return the text of a single recipe block by id, or '' if absent."""
    for seg in _parse_segments(content):
        if seg["kind"] == "recipe" and seg["id"] == rid:
            return "\n".join(seg["lines"])
    return ""


# ── slugify_recipe_id ────────────────────────────────────────────────

class TestSlugifyRecipeId(unittest.TestCase):
    def test_strips_mrdocs_prefix(self):
        self.assertEqual(slugify_recipe_id("MrDocs Version"), "version")

    def test_strips_mrdocs_prefix_case_insensitive(self):
        self.assertEqual(slugify_recipe_id("mrdocs Build Docs"), "build-docs")

    def test_parentheses_and_punctuation(self):
        self.assertEqual(
            slugify_recipe_id("MrDocs Update Test Fixtures (ADOC)"),
            "update-test-fixtures-adoc",
        )

    def test_name_without_mrdocs_prefix(self):
        self.assertEqual(
            slugify_recipe_id("Boost.Url Documentation"),
            "boost-url-documentation",
        )

    def test_bare_mrdocs_keeps_something(self):
        # Stripping would leave nothing, so fall back to the full name.
        self.assertEqual(slugify_recipe_id("MrDocs"), "mrdocs")

    def test_result_is_always_valid(self):
        for name in [
            "MrDocs Version",
            "MrDocs XML Lint with RelaxNG Schema",
            "123 numeric start",
            "!!!",
            "MrDocs",
        ]:
            self.assertRegex(slugify_recipe_id(name), _VALID)

    def test_numeric_start_gets_underscore(self):
        self.assertTrue(slugify_recipe_id("123abc").startswith("_"))


# ── slugify_token ────────────────────────────────────────────────────

class TestSlugifyToken(unittest.TestCase):
    def test_preset_name_unchanged(self):
        self.assertEqual(slugify_token("release-macos"), "release-macos")

    def test_uppercase_lowered(self):
        self.assertEqual(slugify_token("Debug-Linux-GCC-15"), "debug-linux-gcc-15")

    def test_empty(self):
        self.assertEqual(slugify_token(""), "")


# ── recipe_id_for ────────────────────────────────────────────────────

class TestRecipeIdFor(unittest.TestCase):
    def test_explicit_id_honored(self):
        self.assertEqual(recipe_id_for({"name": "MrDocs Version", "id": "ver"}), "ver")

    def test_invalid_explicit_id_slugged(self):
        rid = recipe_id_for({"name": "X", "id": "not a valid id!"})
        self.assertRegex(rid, _VALID)

    def test_auto_slug_when_no_explicit(self):
        self.assertEqual(recipe_id_for({"name": "MrDocs Help"}), "help")


# ── assign_recipe_ids ────────────────────────────────────────────────

class TestAssignRecipeIds(unittest.TestCase):
    def test_unique_ids(self):
        configs = [{"name": "MrDocs Version"}, {"name": "MrDocs Help"}]
        pairs = assign_recipe_ids(configs)
        ids = [rid for _, rid in pairs]
        self.assertEqual(ids, ["version", "help"])

    def test_collision_resolution(self):
        # Two different names that slug to the same id.
        configs = [{"name": "MrDocs Build Docs"}, {"name": "MrDocs build-docs"}]
        pairs = assign_recipe_ids(configs)
        ids = [rid for _, rid in pairs]
        self.assertEqual(ids[0], "build-docs")
        self.assertEqual(ids[1], "build-docs-2")
        self.assertEqual(len(set(ids)), 2)

    def test_suffix_appended(self):
        configs = [{"name": "MrDocs Version"}]
        pairs = assign_recipe_ids(configs, suffix="release-macos")
        self.assertEqual(pairs[0][1], "version-release-macos")

    def test_preserves_order_and_config(self):
        configs = [{"name": "MrDocs Help"}, {"name": "MrDocs Version"}]
        pairs = assign_recipe_ids(configs)
        self.assertIs(pairs[0][0], configs[0])
        self.assertIs(pairs[1][0], configs[1])


# ── recipe emission (US-004) ─────────────────────────────────────────

class TestRecipeEmission(unittest.TestCase):
    def test_target_creates_suffixed_and_agnostic(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs", "args": ["--version"]}])
        self.assertIn("version-release-macos:", content)
        self.assertIn("version: version-release-macos", content)

    def test_script_single_recipe(self):
        content = _gen([{"name": "MrDocs Reformat Source Files", "script": "/d/util/reformat.py"}])
        self.assertIn("reformat-source-files:", content)
        self.assertNotIn("reformat-source-files-release-macos", content)

    def test_default_recipe_present(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}])
        self.assertIn("default:", content)
        self.assertIn("@just --list", content)

    def test_doc_comment_uses_full_name(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}])
        self.assertIn('[doc("MrDocs Version")]', content)

    def test_group_field_maps_to_group_attr(self):
        content = _gen([{"name": "MrDocs Unit Tests", "target": "mrdocs-test",
                         "group": "Test"}])
        self.assertIn('[group("Test")]', content)

    def test_folder_is_not_used_as_group(self):
        # `folder` is the JetBrains concept; it must not become a just group.
        content = _gen([{"name": "MrDocs Update Test Fixtures (ADOC)", "target": "mrdocs-test",
                         "folder": "MrDocs Test Fixtures"}])
        self.assertNotIn('[group("MrDocs Test Fixtures")]', content)

    def test_default_recipe_is_unsorted(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}])
        self.assertIn("@just --list --unsorted", content)

    def test_script_env_and_args(self):
        content = _gen([{"name": "MrDocs Build Docs", "script": "/d/build.sh",
                         "args": ["--clean"], "env": {"MRDOCS_ROOT": "/r"}}])
        block = _recipe_block(content, "build-docs")
        self.assertIn("MRDOCS_ROOT=", block)
        self.assertIn("--clean", block)

    def test_script_cwd_cd(self):
        content = _gen([{"name": "MrDocs Build Docs", "script": "/d/sub/build.sh", "cwd": "/d/sub"}])
        block = _recipe_block(content, "build-docs")
        self.assertIn("cd ", block)
        self.assertIn("/d/sub", block)

    def test_target_recipe_cds_to_build_dir(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs", "args": ["--version"]}])
        block = _recipe_block(content, "version-release-macos")
        # Matches the IDE, which runs target launches with cwd = build_dir.
        self.assertIn("cd ", block)
        self.assertIn("build/release-macos", block)

    def test_configure_and_build_recipes_present(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}])
        self.assertIn("configure:", content)
        self.assertIn("build:", content)
        self.assertIn("cmake --preset release-macos", content)
        self.assertIn("cmake --build", content)

    def test_config_named_build_does_not_clobber_cmake_build(self):
        # A config that slugs to "build" must be suffixed, leaving the cmake
        # build recipes intact.
        content = _gen([{"name": "MrDocs Build", "script": "/d/x.sh"}])
        # The bare `build` recipe stays the cmake delegator, not the script.
        self.assertIn("build: build-release-macos", content)
        self.assertIn("cmake --build", _recipe_block(content, "build-release-macos"))
        # The colliding script recipe still exists under a suffixed id.
        self.assertIn("/d/x.sh", content)
        self.assertNotIn("/d/x.sh", _recipe_block(content, "build"))


# ── build-if-missing precondition (US-009) ───────────────────────────

class TestBuildDependency(unittest.TestCase):
    def test_target_recipe_depends_on_build(self):
        content = _gen([{"name": "Unit Tests", "target": "mrdocs-test"}])
        block = _recipe_block(content, "unit-tests-release-macos")
        # The build prerequisite is a just dependency in the header, so an
        # incremental `cmake --build` keeps the artifact current.
        self.assertIn("unit-tests-release-macos: build-mrdocs-test-release-macos", block)

    def test_run_recipe_has_no_existence_guard(self):
        content = _gen([{"name": "Version", "target": "mrdocs"}])
        block = _recipe_block(content, "version-release-macos")
        self.assertNotIn("test -e", block)
        self.assertNotIn("Test-Path", block)
        self.assertNotIn("cmake --build", block)

    def test_needs_target_adds_build_dependency(self):
        content = _gen([{"name": "Build Docs", "script": "/d/docs/build.sh",
                         "needs_target": "mrdocs"}])
        block = _recipe_block(content, "build-docs")
        self.assertIn("build-docs: build-mrdocs-release-macos", block)
        self.assertNotIn("cmake --build", block)

    def test_script_without_needs_target_has_no_build_dep(self):
        content = _gen([{"name": "Reformat", "script": "/d/util/reformat.py"}])
        block = _recipe_block(content, "reformat")
        self.assertNotIn("cmake --build", block)
        header = next(l for l in block.splitlines() if l.startswith("reformat:"))
        self.assertEqual(header, "reformat:")


# ── merge-in-place (US-006) ──────────────────────────────────────────

class TestMerge(unittest.TestCase):
    def test_preserves_user_recipe(self):
        existing = ('set shell := ["bash", "-uc"]\n\n'
                    '# my own recipe\nmy-thing:\n    echo hello\n')
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}], existing=existing)
        self.assertIn("my-thing:", content)
        self.assertIn("echo hello", content)
        self.assertIn("version-release-macos:", content)

    def test_overwrites_generated_no_duplicate(self):
        cfgs = [{"name": "MrDocs Version", "target": "mrdocs"}]
        first = _gen(cfgs)
        second = _gen(cfgs, existing=first)
        headers = re.findall(r"(?m)^version-release-macos:", second)
        self.assertEqual(len(headers), 1)

    def test_readds_deleted_generated_recipe(self):
        existing = 'set shell := ["bash", "-uc"]\n\n# user\nmine:\n    echo hi\n'
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}], existing=existing)
        self.assertIn("version-release-macos:", content)
        self.assertIn("mine:", content)

    def test_set_directive_not_misread_as_recipe(self):
        existing = 'set shell := ["bash", "-uc"]\nset positional-arguments\n'
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}], existing=existing)
        self.assertIn('set shell := ["bash", "-uc"]', content)
        self.assertIn("set positional-arguments", content)


# ── pruning (US-007) ─────────────────────────────────────────────────

class TestPrune(unittest.TestCase):
    def test_prunes_recipe_for_removed_preset(self):
        # version is a generated base; the debug-macos variant is no longer a
        # valid preset, so it is pruned by name (no marker needed).
        existing = '[doc("Old")]\nversion-debug-macos:\n    echo stale\n'
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}],
                       all_presets=["release-macos"], existing=existing)
        self.assertNotIn("version-debug-macos:", content)
        self.assertNotIn("echo stale", content)

    def test_keeps_recipe_for_other_valid_preset(self):
        existing = '[doc("MrDocs Version")]\nversion-release-linux:\n    echo keepme\n'
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}],
                       preset="release-macos",
                       all_presets=["release-macos", "release-linux"],
                       existing=existing)
        self.assertIn("version-release-linux:", content)
        self.assertIn("echo keepme", content)

    def test_does_not_prune_distinct_user_recipe(self):
        existing = "# user\nmy-custom-helper:\n    echo mine\n"
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}],
                       all_presets=["release-macos"], existing=existing)
        self.assertIn("my-custom-helper:", content)
        self.assertIn("echo mine", content)


# ── install hint (US-010) ────────────────────────────────────────────

class TestInstallHint(unittest.TestCase):
    @patch("src.configs.justfile.shutil.which", return_value=None)
    def test_hint_printed_when_just_missing(self, _mock_which):
        d = tempfile.mkdtemp()
        ui = MagicMock()
        generate_justfile_run_configs(
            [{"name": "MrDocs Version", "target": "mrdocs"}],
            source_dir=d, build_dir=os.path.join(d, "build", "release-macos"),
            preset="release-macos", dry_run=False, ui=ui,
            all_presets=["release-macos"], default_preset="release-macos",
            justfile_path=os.path.join(d, "justfile"),
        )
        printed = " ".join(str(c) for c in ui.info.call_args_list)
        self.assertIn("just", printed)
        self.assertTrue(os.path.exists(os.path.join(d, "justfile")))

    @patch("src.configs.justfile.shutil.which", return_value="/opt/just")
    def test_no_hint_when_just_present(self, _mock_which):
        d = tempfile.mkdtemp()
        ui = MagicMock()
        generate_justfile_run_configs(
            [{"name": "MrDocs Version", "target": "mrdocs"}],
            source_dir=d, build_dir=os.path.join(d, "build", "release-macos"),
            preset="release-macos", dry_run=False, ui=ui,
            all_presets=["release-macos"], default_preset="release-macos",
            justfile_path=os.path.join(d, "justfile"),
        )
        printed = " ".join(str(c) for c in ui.info.call_args_list)
        self.assertNotIn("was not found on PATH", printed)


# ── scanner robustness ───────────────────────────────────────────────

class TestParseSegments(unittest.TestCase):
    def test_recipe_detected(self):
        segs = _parse_segments("foo:\n    echo hi\n")
        recipes = [s for s in segs if s["kind"] == "recipe"]
        self.assertEqual(len(recipes), 1)
        self.assertEqual(recipes[0]["id"], "foo")

    def test_assignment_not_a_recipe(self):
        segs = _parse_segments('x := "y"\n')
        self.assertTrue(all(s["kind"] == "raw" for s in segs))

    def test_no_marker_in_output(self):
        # The old "# mrdocs:generated" marker must not appear anywhere.
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}])
        self.assertNotIn("mrdocs:generated", content)

    def test_mrdocs_alias_defined_and_used(self):
        content = _gen([{"name": "MrDocs Version", "target": "mrdocs"}])
        self.assertIn("mrdocs := justfile_directory()", content)
        self.assertIn("{{mrdocs}}", content)


class TestStaleVariant(unittest.TestCase):
    def test_stale_when_token_invalid(self):
        self.assertTrue(_is_stale_variant("version-debug-macos", {"version"}, {"release-macos"}))

    def test_not_stale_when_token_valid(self):
        self.assertFalse(_is_stale_variant("version-release-macos", {"version"}, {"release-macos"}))

    def test_longest_base_wins(self):
        # build-mrdocs-test-release-macos: the build-mrdocs-test base yields a
        # valid token, so it is kept even though shorter bases would not.
        bases = {"build", "build-mrdocs", "build-mrdocs-test"}
        self.assertFalse(_is_stale_variant("build-mrdocs-test-release-macos", bases, {"release-macos"}))

    def test_unrelated_recipe_not_stale(self):
        self.assertFalse(_is_stale_variant("my-helper", {"version", "build"}, {"release-macos"}))


# ── interface target / aggregate (US: test) ──────────────────────────

class TestAggregateRecipe(unittest.TestCase):
    def test_depends_recipe_lists_dependencies(self):
        configs = [
            {"name": "MrDocs Unit Tests", "target": "mrdocs-test"},
            {"name": "MrDocs Test Test Fixtures (ADOC)", "target": "mrdocs-test"},
            {"name": "MrDocs Test", "id": "test",
             "depends": ["MrDocs Unit Tests", "MrDocs Test Test Fixtures (ADOC)"]},
        ]
        content = _gen(configs)
        block = _recipe_block(content, "test")
        self.assertIn("test: unit-tests test-test-fixtures-adoc", block)
        # The interface target has no body of its own; it only depends.
        self.assertNotIn("cmake", block)

    def test_depends_drops_filtered_out_recipes(self):
        # If a dependency config is absent, it is simply not listed.
        configs = [
            {"name": "MrDocs Unit Tests", "target": "mrdocs-test"},
            {"name": "MrDocs Test", "id": "test",
             "depends": ["MrDocs Unit Tests", "MrDocs Missing Thing"]},
        ]
        content = _gen(configs)
        block = _recipe_block(content, "test")
        self.assertIn("test: unit-tests", block)
        self.assertNotIn("missing-thing", block)


class TestOrdering(unittest.TestCase):
    def _gen_grouped(self, **kw):
        configs = [
            {"name": "MrDocs Unit Tests", "target": "mrdocs-test", "group": "Test"},
            {"name": "MrDocs Test", "id": "test", "group": "Test",
             "depends": ["MrDocs Unit Tests"]},
            {"name": "MrDocs Version", "target": "mrdocs", "group": "Run"},
        ]
        return _gen(configs, group_order=["Test", "Run", "Build"], **kw)

    def test_groups_appear_in_group_order(self):
        content = self._gen_grouped()
        self.assertLess(content.index("[group(\"Test\")]"), content.index("[group(\"Run\")]"))
        self.assertLess(content.index("[group(\"Run\")]"), content.index("[group(\"Build\")]"))

    def test_dependent_before_dependency_within_group(self):
        content = self._gen_grouped()
        # `test` depends on `unit-tests`, so it must appear first.
        self.assertLess(content.index("\ntest:"), content.index("\nunit-tests:"))

    def test_build_recipes_come_after_run_and_test(self):
        content = self._gen_grouped()
        # Build is a dependency of test/version, so its group comes last.
        self.assertGreater(content.index("[group(\"Build\")]"),
                           content.index("unit-tests-release-macos:"))


class TestOrderRecipesUnit(unittest.TestCase):
    def test_topo_dependents_first(self):
        from src.configs.justfile import _order_recipes
        recipes = [
            {"id": "configure-x", "block": ["configure-x:"], "group": "Build", "deps": []},
            {"id": "build-x", "block": ["build-x:"], "group": "Build", "deps": ["configure-x"]},
        ]
        ordered = [r["id"] for r in _order_recipes(recipes, ["Build"])]
        self.assertLess(ordered.index("build-x"), ordered.index("configure-x"))


class TestBuildRecipes(unittest.TestCase):
    def test_per_target_build_recipe_exists(self):
        content = _gen([{"name": "Unit Tests", "target": "mrdocs-test"}])
        block = _recipe_block(content, "build-mrdocs-test-release-macos")
        self.assertIn("cmake --build", block)
        self.assertIn("--target mrdocs-test", block)

    def test_build_recipe_configure_guard_calls_configure_recipe(self):
        content = _gen([{"name": "Unit Tests", "target": "mrdocs-test"}])
        block = _recipe_block(content, "build-mrdocs-test-release-macos")
        # The configure-if-needed guard delegates to the configure recipe.
        self.assertIn("just configure-release-macos", block)
        # cmake --preset lives only in the configure recipe, not inlined here.
        self.assertNotIn("cmake --preset", block)

    def test_cmake_preset_lives_in_configure_recipe(self):
        content = _gen([{"name": "Unit Tests", "target": "mrdocs-test"}])
        self.assertIn("cmake --preset release-macos",
                      _recipe_block(content, "configure-release-macos"))


if __name__ == "__main__":
    unittest.main()
