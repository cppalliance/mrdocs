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
Recipe schema definitions.

Contains dataclasses representing recipe structures for third-party
dependency management.
"""

from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any


@dataclass
class RecipeSource:
    """
    Source specification for a recipe.

    Defines where to obtain the source code for a dependency.
    """
    type: str  # "git", "archive", "http", "zip"
    url: str
    branch: Optional[str] = None
    tag: Optional[str] = None
    commit: Optional[str] = None
    ref: Optional[str] = None
    depth: Optional[int] = None
    submodules: bool = False


@dataclass
class Recipe:
    """
    A recipe for building a third-party dependency.

    Recipes define how to fetch, configure, build, and install
    dependencies required by MrDocs.
    """
    name: str
    version: str
    source: RecipeSource
    dependencies: List[str]
    source_dir: str
    build_dir: str
    install_dir: str
    build_type: str
    source_subdir: Optional[str] = None
    build: List[Dict[str, Any]] = field(default_factory=list)
    tags: List[str] = field(default_factory=list)
    package_root_var: Optional[str] = None
    install_scope: str = "per-preset"  # "per-preset" (default) or "global"
