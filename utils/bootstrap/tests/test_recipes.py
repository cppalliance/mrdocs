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

"""Tests for recipe schema and utilities."""

import sys
import unittest

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

from src.recipes.schema import RecipeSource, Recipe
from src.recipes.loader import topo_sort_recipes, apply_placeholders


class TestRecipeSource(unittest.TestCase):
    """Test RecipeSource dataclass."""

    def test_git_source(self):
        """Should create a git source."""
        source = RecipeSource(
            type="git",
            url="https://github.com/example/repo.git",
            branch="main"
        )
        self.assertEqual(source.type, "git")
        self.assertEqual(source.url, "https://github.com/example/repo.git")
        self.assertEqual(source.branch, "main")

    def test_archive_source(self):
        """Should create an archive source."""
        source = RecipeSource(
            type="archive",
            url="https://example.com/file.tar.gz"
        )
        self.assertEqual(source.type, "archive")
        self.assertIsNone(source.branch)

    def test_git_source_with_commit(self):
        """Should create a git source with specific commit."""
        source = RecipeSource(
            type="git",
            url="https://github.com/example/repo.git",
            commit="abc123def456"
        )
        self.assertEqual(source.commit, "abc123def456")

    def test_git_source_with_tag(self):
        """Should create a git source with tag."""
        source = RecipeSource(
            type="git",
            url="https://github.com/example/repo.git",
            tag="v1.0.0"
        )
        self.assertEqual(source.tag, "v1.0.0")

    def test_submodules_default_false(self):
        """Submodules should default to False."""
        source = RecipeSource(type="git", url="https://example.com/repo.git")
        self.assertFalse(source.submodules)


class TestRecipe(unittest.TestCase):
    """Test Recipe dataclass."""

    def test_basic_recipe(self):
        """Should create a basic recipe."""
        source = RecipeSource(type="git", url="https://example.com/repo.git")
        recipe = Recipe(
            name="test-lib",
            version="1.0.0",
            source=source,
            dependencies=[],
            source_dir="/src/test-lib",
            build_dir="/build/test-lib",
            install_dir="/install/test-lib",
            build_type="Release"
        )
        self.assertEqual(recipe.name, "test-lib")
        self.assertEqual(recipe.version, "1.0.0")
        self.assertEqual(recipe.build_type, "Release")

    def test_recipe_with_dependencies(self):
        """Should create a recipe with dependencies."""
        source = RecipeSource(type="git", url="https://example.com/repo.git")
        recipe = Recipe(
            name="depends-on-others",
            version="2.0.0",
            source=source,
            dependencies=["lib-a", "lib-b"],
            source_dir="/src/lib",
            build_dir="/build/lib",
            install_dir="/install/lib",
            build_type="Debug"
        )
        self.assertEqual(len(recipe.dependencies), 2)
        self.assertIn("lib-a", recipe.dependencies)
        self.assertIn("lib-b", recipe.dependencies)

    def test_default_install_scope(self):
        """Default install_scope should be per-preset."""
        source = RecipeSource(type="git", url="https://example.com/repo.git")
        recipe = Recipe(
            name="test",
            version="1.0",
            source=source,
            dependencies=[],
            source_dir="/src",
            build_dir="/build",
            install_dir="/install",
            build_type="Release"
        )
        self.assertEqual(recipe.install_scope, "per-preset")

    def test_default_build_steps_empty(self):
        """Default build steps should be empty list."""
        source = RecipeSource(type="git", url="https://example.com/repo.git")
        recipe = Recipe(
            name="test",
            version="1.0",
            source=source,
            dependencies=[],
            source_dir="/src",
            build_dir="/build",
            install_dir="/install",
            build_type="Release"
        )
        self.assertEqual(recipe.build, [])


class TestTopoSortRecipes(unittest.TestCase):
    """Test topological sorting of recipes."""

    def _make_recipe(self, name, dependencies=None):
        """Helper to create a recipe with given name and dependencies."""
        source = RecipeSource(type="git", url=f"https://example.com/{name}.git")
        return Recipe(
            name=name,
            version="1.0",
            source=source,
            dependencies=dependencies or [],
            source_dir=f"/src/{name}",
            build_dir=f"/build/{name}",
            install_dir=f"/install/{name}",
            build_type="Release"
        )

    def test_no_dependencies(self):
        """Recipes without dependencies should maintain order."""
        recipes = [
            self._make_recipe("a"),
            self._make_recipe("b"),
            self._make_recipe("c"),
        ]
        sorted_recipes = topo_sort_recipes(recipes)
        self.assertEqual(len(sorted_recipes), 3)

    def test_simple_dependency(self):
        """Dependency should come before dependent."""
        recipes = [
            self._make_recipe("b", ["a"]),
            self._make_recipe("a"),
        ]
        sorted_recipes = topo_sort_recipes(recipes)
        names = [r.name for r in sorted_recipes]
        self.assertLess(names.index("a"), names.index("b"))

    def test_chain_dependencies(self):
        """Chain of dependencies should be properly ordered."""
        recipes = [
            self._make_recipe("c", ["b"]),
            self._make_recipe("b", ["a"]),
            self._make_recipe("a"),
        ]
        sorted_recipes = topo_sort_recipes(recipes)
        names = [r.name for r in sorted_recipes]
        self.assertLess(names.index("a"), names.index("b"))
        self.assertLess(names.index("b"), names.index("c"))

    def test_diamond_dependencies(self):
        """Diamond dependency pattern should work."""
        # d depends on b and c, both depend on a
        recipes = [
            self._make_recipe("d", ["b", "c"]),
            self._make_recipe("c", ["a"]),
            self._make_recipe("b", ["a"]),
            self._make_recipe("a"),
        ]
        sorted_recipes = topo_sort_recipes(recipes)
        names = [r.name for r in sorted_recipes]
        # a must come before b and c
        self.assertLess(names.index("a"), names.index("b"))
        self.assertLess(names.index("a"), names.index("c"))
        # b and c must come before d
        self.assertLess(names.index("b"), names.index("d"))
        self.assertLess(names.index("c"), names.index("d"))

    def test_empty_list(self):
        """Empty recipe list should return empty list."""
        self.assertEqual(topo_sort_recipes([]), [])


class TestApplyPlaceholders(unittest.TestCase):
    """Test placeholder substitution."""

    def test_simple_substitution(self):
        """Should substitute simple placeholders."""
        result = apply_placeholders(
            "${name}-${version}",
            {"name": "mylib", "version": "1.0"}
        )
        self.assertEqual(result, "mylib-1.0")

    def test_no_placeholders(self):
        """String without placeholders should be unchanged."""
        result = apply_placeholders("no placeholders here", {"key": "value"})
        self.assertEqual(result, "no placeholders here")

    def test_missing_placeholder(self):
        """Missing placeholder should remain unchanged."""
        result = apply_placeholders("${exists}-${missing}", {"exists": "value"})
        self.assertEqual(result, "value-${missing}")

    def test_empty_string(self):
        """Empty string should return empty string."""
        result = apply_placeholders("", {"key": "value"})
        self.assertEqual(result, "")


if __name__ == "__main__":
    unittest.main()
