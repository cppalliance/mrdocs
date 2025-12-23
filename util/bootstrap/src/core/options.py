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
Configuration options for the bootstrap installer.

Contains the InstallOptions dataclass that stores all configuration
settings for the MrDocs bootstrap process.
"""

import os
from dataclasses import dataclass, field

from .platform import running_from_mrdocs_source_dir, get_source_dir, is_windows


@dataclass
class InstallOptions:
    """
    Stores configuration options for the MrDocs bootstrap installer.

    The @dataclass decorator automatically generates __init__, __repr__,
    and __eq__ methods based on the class attributes. This simplifies
    creation of classes primarily used to store data.
    """
    # Compiler
    cc: str = ''
    cxx: str = ''
    sanitizer: str = ''

    # Required tools
    git_path: str = ''
    cmake_path: str = ''
    python_path: str = ''

    # Test tools
    java_path: str = ''

    # Optional tools
    ninja_path: str = ''

    # MrDocs
    source_dir: str = field(default_factory=get_source_dir)
    build_type: str = "Release"
    preset: str = "<build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    build_dir: str = "<source-dir>/build/<build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower><\"-\":if(sanitizer)><sanitizer:lower>"
    build_tests: bool = True
    system_install: bool = False
    install_dir: str = "<source-dir>/install/<build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    run_tests: bool = False

    # Third-party dependencies root and recipes
    third_party_src_dir: str = "<source-dir>/build/third-party"

    # Information to create run configurations
    generate_run_configs: bool = True
    jetbrains_run_config_dir: str = "<source-dir>/.run"
    boost_src_dir: str = "<source-dir>/../boost"
    generate_clion_run_configs: bool = True
    generate_vscode_run_configs: bool = field(default_factory=lambda: not is_windows())
    generate_vs_run_configs: bool = field(default_factory=is_windows)

    # Information to create pretty printer configs
    generate_pretty_printer_configs: bool = True

    # Command line arguments
    non_interactive: bool = False
    refresh_all: bool = False
    force_rebuild: bool = False
    remove_build_dir: bool = True
    plain_ui: bool = False
    verbose: bool = False
    debug: bool = False
    dry_run: bool = False
    list_recipes: bool = False
    recipe_filter: str = ""
    skip_build: bool = False
    clean: bool = False
    force: bool = False
    refresh_all: bool = False


# Valid build types (user-facing; OptimizedDebug is internal-only for MSVC + DebugFast)
BUILD_TYPES = ["Release", "Debug", "RelWithDebInfo", "MinSizeRel", "DebugFast"]

# Valid sanitizers
SANITIZERS = ["address", "undefined", "thread", "memory", ""]
