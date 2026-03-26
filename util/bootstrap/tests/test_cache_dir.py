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

"""Tests for --cache-dir functionality."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.core.options import InstallOptions
from src.recipes.schema import Recipe, RecipeSource
from src.recipes.fetcher import (
    is_recipe_up_to_date,
    write_recipe_stamp,
    recipe_stamp_path,
)


def make_recipe(name="llvm", version="abc123", install_dir="/tmp/test"):
    """Create a minimal Recipe for testing."""
    return Recipe(
        name=name,
        version=version,
        source=RecipeSource(
            type="git",
            url="https://github.com/llvm/llvm-project.git",
            commit="abc123def456",
        ),
        dependencies=[],
        source_dir="/tmp/source/" + name,
        build_dir="/tmp/build/" + name,
        install_dir=install_dir,
        build_type="Release",
        build=[],
        tags=[],
        package_root_var=f"{name.upper()}_ROOT",
    )


class TestCacheDirOption(unittest.TestCase):
    """Test that cache_dir field exists and works in InstallOptions."""

    def test_default_cache_dir_empty(self):
        """Default cache_dir should be empty string."""
        opts = InstallOptions()
        self.assertEqual(opts.cache_dir, "")

    def test_override_cache_dir(self):
        """Should be able to set cache_dir."""
        opts = InstallOptions(cache_dir="/tmp/cache")
        self.assertEqual(opts.cache_dir, "/tmp/cache")

    def test_cache_dir_falsy_when_empty(self):
        """Empty cache_dir should be falsy."""
        opts = InstallOptions()
        self.assertFalse(opts.cache_dir)


class TestCacheDirCLI(unittest.TestCase):
    """Test --cache-dir CLI argument parsing."""

    @classmethod
    def setUpClass(cls):
        from src.__main__ import get_command_line_args
        cls.get_command_line_args = staticmethod(get_command_line_args)

    def test_cache_dir_parsed(self):
        """--cache-dir should be captured in parsed args."""
        args = self.get_command_line_args(["--cache-dir", "/tmp/cache"])
        self.assertEqual(args["cache_dir"], "/tmp/cache")

    def test_cache_dir_not_present_when_omitted(self):
        """cache_dir should not appear in args when omitted."""
        args = self.get_command_line_args(["--build-type", "Debug"])
        self.assertNotIn("cache_dir", args)

    def test_cache_dir_with_other_options(self):
        """--cache-dir should coexist with other options."""
        args = self.get_command_line_args([
            "--cache-dir", "../third-party",
            "--yes",
            "--plain",
            "--build-type", "Release",
        ])
        self.assertEqual(args["cache_dir"], "../third-party")
        self.assertTrue(args["non_interactive"])
        self.assertTrue(args["plain_ui"])


class TestCacheDirInstallOverride(unittest.TestCase):
    """Test that cache_dir overrides recipe install directories."""

    def test_install_dir_overridden(self):
        """When cache_dir is set, recipe install_dir should be <cache_dir>/<name>."""
        cache_dir = "/tmp/my-cache"
        recipe = make_recipe(name="llvm", install_dir="/original/path/llvm")

        # Simulate what installer.py does
        recipe.install_dir = os.path.join(os.path.abspath(cache_dir), recipe.name)

        self.assertTrue(recipe.install_dir.endswith("/llvm"))
        self.assertIn("my-cache", recipe.install_dir)

    def test_multiple_recipes_get_different_dirs(self):
        """Each recipe should get its own subdirectory under cache_dir."""
        cache_dir = os.path.abspath("/tmp/cache")
        recipes = [
            make_recipe(name="llvm"),
            make_recipe(name="boost_mp11"),
            make_recipe(name="libxml2"),
        ]
        for r in recipes:
            r.install_dir = os.path.join(cache_dir, r.name)

        self.assertEqual(recipes[0].install_dir, os.path.join(cache_dir, "llvm"))
        self.assertEqual(recipes[1].install_dir, os.path.join(cache_dir, "boost_mp11"))
        self.assertEqual(recipes[2].install_dir, os.path.join(cache_dir, "libxml2"))


class TestStampFileInCacheDir(unittest.TestCase):
    """Test stamp file operations in cache directory context."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_stamp_written_to_cache_dir(self):
        """Stamp file should be written inside the recipe's install_dir."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        recipe = make_recipe(name="llvm", install_dir=install_dir)
        write_recipe_stamp(recipe, "abc123def456")

        stamp_path = recipe_stamp_path(recipe)
        self.assertTrue(os.path.exists(stamp_path))
        self.assertTrue(stamp_path.startswith(install_dir))

    def test_stamp_makes_recipe_up_to_date(self):
        """After writing a stamp, is_recipe_up_to_date should return True."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        recipe = make_recipe(name="llvm", version="abc123", install_dir=install_dir)
        resolved_ref = "abc123def456"

        self.assertNotEqual(is_recipe_up_to_date(recipe, resolved_ref), "")
        write_recipe_stamp(recipe, resolved_ref)
        self.assertEqual(is_recipe_up_to_date(recipe, resolved_ref), "")

    def test_stamp_version_mismatch(self):
        """Stamp with different version should not be up to date."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        recipe = make_recipe(name="llvm", version="abc123", install_dir=install_dir)
        write_recipe_stamp(recipe, "old_ref")

        self.assertNotEqual(is_recipe_up_to_date(recipe, "new_ref"), "")


class TestLegacyCacheDetection(unittest.TestCase):
    """Test detection of existing CI caches without bootstrap stamp files."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_empty_dir_not_legacy_cache(self):
        """Empty directory should not be treated as legacy cache."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        os.makedirs(install_dir)
        # Empty dir has no files
        self.assertEqual(len(os.listdir(install_dir)), 0)

    def test_nonempty_dir_without_stamp_is_legacy(self):
        """Non-empty dir without stamp file is a legacy cache."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        os.makedirs(os.path.join(install_dir, "bin"), exist_ok=True)
        # Create a dummy file to simulate LLVM install
        with open(os.path.join(install_dir, "bin", "clang"), "w") as f:
            f.write("dummy")

        recipe = make_recipe(name="llvm", install_dir=install_dir)
        resolved_ref = "abc123"

        # Should NOT be up to date (no stamp)
        self.assertNotEqual(is_recipe_up_to_date(recipe, resolved_ref), "")
        # But directory is non-empty
        self.assertTrue(os.path.isdir(install_dir))
        self.assertTrue(len(os.listdir(install_dir)) > 0)

    def test_legacy_cache_gets_stamp_after_write(self):
        """Writing stamp to legacy cache makes it detectable on next run."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        os.makedirs(os.path.join(install_dir, "lib"), exist_ok=True)
        with open(os.path.join(install_dir, "lib", "libLLVM.so"), "w") as f:
            f.write("dummy")

        recipe = make_recipe(name="llvm", version="abc123", install_dir=install_dir)
        resolved_ref = "abc123def456"

        # Initially no stamp
        self.assertNotEqual(is_recipe_up_to_date(recipe, resolved_ref), "")

        # Write stamp (simulating what installer does for legacy caches)
        write_recipe_stamp(recipe, resolved_ref)

        # Now it should be up to date
        self.assertEqual(is_recipe_up_to_date(recipe, resolved_ref), "")

    def test_ci_compatible_cache_path(self):
        """Cache dir structure matches CI: <cache-dir>/<recipe-name>/."""
        cache_dir = os.path.join(self.tmpdir, "third-party")
        os.makedirs(cache_dir)

        # CI caches LLVM at ../third-party/llvm/
        recipe = make_recipe(name="llvm")
        recipe.install_dir = os.path.join(cache_dir, recipe.name)

        expected = os.path.join(cache_dir, "llvm")
        self.assertEqual(recipe.install_dir, expected)

    def test_stamp_content_format(self):
        """Stamp file should contain name, version, and ref as JSON."""
        install_dir = os.path.join(self.tmpdir, "llvm")
        recipe = make_recipe(name="llvm", version="v18.1.0", install_dir=install_dir)
        resolved_ref = "abc123def456"

        write_recipe_stamp(recipe, resolved_ref)

        stamp_path = recipe_stamp_path(recipe)
        with open(stamp_path, "r") as f:
            data = json.load(f)

        self.assertEqual(data["name"], "llvm")
        self.assertEqual(data["version"], "v18.1.0")
        self.assertEqual(data["ref"], "abc123def456")


if __name__ == "__main__":
    unittest.main()
