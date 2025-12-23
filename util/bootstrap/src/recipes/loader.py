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

"""
Recipe loading and processing utilities.

Provides functions to load recipe JSON files and process them for
building dependencies.
"""

import json
import os
from typing import List, Dict, Any, Optional

from ..core.platform import is_windows
from ..core.ui import TextUI, get_default_ui
from .schema import Recipe, RecipeSource


def recipe_placeholders(
    recipe: Recipe,
    preset: str,
    cc: str = "",
    cxx: str = "",
    build_dir: str = "",
    install_dir: str = "",
) -> Dict[str, str]:
    """
    Get placeholder values for a recipe.

    Args:
        recipe: The recipe to get placeholders for.
        preset: The build preset name.
        cc: C compiler path.
        cxx: C++ compiler path.
        build_dir: Project build directory.
        install_dir: Project install directory.

    Returns:
        Dictionary mapping placeholder names to values.
    """
    host_suffix = "windows" if is_windows() else "unix"
    return {
        "BOOTSTRAP_BUILD_TYPE": recipe.build_type,
        "BOOTSTRAP_BUILD_TYPE_LOWER": recipe.build_type.lower(),
        "BOOTSTRAP_CONFIGURE_PRESET": preset,
        "BOOTSTRAP_CC": cc,
        "BOOTSTRAP_CXX": cxx,
        "BOOTSTRAP_PROJECT_BUILD_DIR": build_dir,
        "BOOTSTRAP_PROJECT_INSTALL_DIR": install_dir,
        "BOOTSTRAP_HOST_PRESET_SUFFIX": host_suffix,
        "build_type": recipe.build_type,
        "build_type_lower": recipe.build_type.lower(),
    }


def apply_placeholders(value: Any, placeholders: Dict[str, str]) -> Any:
    """
    Apply placeholder substitutions to a value.

    Args:
        value: Value to process (string, list, or dict).
        placeholders: Dictionary mapping placeholder names to values.

    Returns:
        Processed value with placeholders substituted.
    """
    if isinstance(value, str):
        for k, v in placeholders.items():
            value = value.replace("${" + k + "}", v)
        return value
    if isinstance(value, list):
        return [apply_placeholders(v, placeholders) for v in value]
    if isinstance(value, dict):
        return {
            apply_placeholders(k, placeholders): apply_placeholders(v, placeholders)
            for k, v in value.items()
        }
    return value


def expand_path(
    template: str,
    source_dir: str,
    third_party_src_dir: str,
    build_type: str,
) -> str:
    """
    Expand path template variables.

    Args:
        template: Path template string.
        source_dir: MrDocs source directory.
        third_party_src_dir: Third-party sources directory.
        build_type: Build type (Release, Debug, etc.).

    Returns:
        Expanded absolute path.
    """
    if not template:
        return template

    build_lower = build_type.lower() if build_type else ""
    repl = {
        "${source_dir}": source_dir,
        "${third_party_src_dir}": third_party_src_dir,
        "${build_type}": build_type,
        "${build_type_lower}": build_lower,
    }

    out = template
    for k, v in repl.items():
        out = out.replace(k, v)

    if not os.path.isabs(out):
        out = os.path.normpath(os.path.join(source_dir, out))

    return out


def load_recipe_files(
    recipes_dir: str,
    source_dir: str,
    preset: str,
    build_type: str,
    cc: str = "",
    cxx: str = "",
    build_dir: str = "",
    install_dir: str = "",
    ui: Optional[TextUI] = None,
) -> List[Recipe]:
    """
    Load recipe files from the recipes directory.

    Args:
        recipes_dir: Directory containing recipe JSON files.
        source_dir: MrDocs source directory.
        preset: Build preset name.
        build_type: Build type (Release, Debug, etc.).
        cc: C compiler path.
        cxx: C++ compiler path.
        build_dir: Project build directory.
        install_dir: Project install directory.
        ui: TextUI instance for output.

    Returns:
        List of Recipe objects.
    """
    if ui is None:
        ui = get_default_ui()

    if not os.path.isdir(recipes_dir):
        return []

    # For debug-fast, dependencies reuse release (or optimized debug on Windows) builds/presets.
    dep_build_type = "OptimizedDebug" if is_windows() else "Release"
    dep_preset = preset
    if build_type.lower() in ("debugfast", "debug-fast"):
        if "debug-fast" in dep_preset:
            dep_preset = dep_preset.replace("debug-fast", dep_build_type.lower())
        elif "debugfast" in dep_preset:
            dep_preset = dep_preset.replace("debugfast", dep_build_type.lower())

    recipes: List[Recipe] = []
    for path in sorted(os.listdir(recipes_dir)):
        if not path.endswith(".json"):
            continue
        full = os.path.join(recipes_dir, path)
        try:
            with open(full, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception as exc:
            ui.warn(f"Skipping recipe {path}: {exc}")
            continue

        src = data.get("source", {})
        recipe = Recipe(
            name=data.get("name") or os.path.splitext(path)[0],
            version=str(data.get("version", "")),
            source=RecipeSource(
                type=src.get("type", "git"),
                url=src.get("url", ""),
                branch=src.get("branch"),
                tag=src.get("tag"),
                commit=src.get("commit"),
                ref=src.get("ref"),
                depth=src.get("depth"),
                submodules=bool(src.get("submodules", False)),
            ),
            dependencies=data.get("dependencies", []),
            source_dir=data.get("source_dir", ""),
            build_dir=data.get("build_dir", ""),
            install_dir=data.get("install_dir", ""),
            build_type=data.get("build_type", "Release"),
            build=data.get("build", []),
            tags=data.get("tags", []),
            package_root_var=data.get("package_root_var"),
            install_scope=data.get("install_scope", "per-preset"),
        )

        placeholders = recipe_placeholders(
            recipe, preset, cc, cxx, build_dir, install_dir
        )

        # Apply placeholders to source reference fields
        recipe.source.url = apply_placeholders(recipe.source.url, placeholders)
        recipe.source.branch = apply_placeholders(recipe.source.branch, placeholders)
        recipe.source.tag = apply_placeholders(recipe.source.tag, placeholders)
        recipe.source.commit = apply_placeholders(recipe.source.commit, placeholders)
        recipe.source.ref = apply_placeholders(recipe.source.ref, placeholders)

        # Paths are controlled by bootstrap, not the recipe file.
        tp_root = os.path.join(source_dir, "build", "third-party")
        if recipe.install_scope == "global":
            recipe.source_dir = os.path.join(tp_root, "source", recipe.name)
            recipe.build_dir = os.path.join(tp_root, "build", recipe.name)
            recipe.install_dir = os.path.join(tp_root, "install", recipe.name)
        else:
            recipe.source_dir = os.path.join(tp_root, "source", recipe.name)
            recipe.build_dir = os.path.join(tp_root, "build", dep_preset, recipe.name)
            recipe.install_dir = os.path.join(tp_root, "install", dep_preset, recipe.name)

        recipes.append(recipe)

    return recipes


def topo_sort_recipes(recipes: List[Recipe]) -> List[Recipe]:
    """
    Topologically sort recipes by dependencies.

    Args:
        recipes: List of recipes to sort.

    Returns:
        Sorted list of recipes (dependencies first).

    Raises:
        RuntimeError: If there is a dependency cycle or missing dependency.
    """
    by_name = {r.name: r for r in recipes}
    visited: Dict[str, bool] = {}
    order: List[Recipe] = []

    def visit(name: str, stack: List[str]):
        state = visited.get(name)
        if state is True:
            return
        if state is False:
            raise RuntimeError(f"Dependency cycle: {' -> '.join(stack + [name])}")
        visited[name] = False
        stack.append(name)
        for dep in by_name[name].dependencies:
            if dep not in by_name:
                raise RuntimeError(f"Missing dependency recipe '{dep}' needed by '{name}'")
            visit(dep, stack)
        visited[name] = True
        stack.pop()
        order.append(by_name[name])

    for n in by_name:
        if visited.get(n) is not True:
            visit(n, [])

    return order
