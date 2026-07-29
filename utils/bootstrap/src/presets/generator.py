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
CMake presets generator.

Provides functions to create and update CMakeUserPresets.json files.
"""

import json
import os
from typing import Optional, Dict, Any, List, Tuple

from ..core.platform import is_windows, is_macos
from ..core.filesystem import write_text, is_executable
from ..core.ui import TextUI, get_default_ui
from ..tools.compilers import sanitizer_flag_name
from functools import lru_cache


@lru_cache(maxsize=1)
def get_host_system_name() -> Tuple[str, str]:
    """
    Get the host system name for CMake presets.

    Returns:
        Tuple of (hostSystemName, displayName).
    """
    if is_windows():
        return "Windows", "Windows"
    elif is_macos():
        return "Darwin", "macOS"
    else:
        return "Linux", "Linux"


def get_parent_preset_name(build_type: str) -> str:
    """
    Get the parent preset name based on build type.

    Args:
        build_type: The build type (Debug, Release, etc.).

    Returns:
        Parent preset name to inherit from.
    """
    bt_lower = build_type.lower()
    if bt_lower not in ("debug", "debugfast", "debug-fast"):
        if bt_lower == "relwithdebinfo":
            return "relwithdebinfo"
        return "release"
    return "debug"


def get_display_name(
    build_type: str,
    os_display_name: str,
    cc: str = "",
    sanitizer: str = "",
) -> str:
    """
    Generate a display name for a preset.

    Args:
        build_type: Build type.
        os_display_name: OS display name.
        cc: C compiler path.
        sanitizer: Sanitizer name.

    Returns:
        Human-readable preset display name.
    """
    display_name = build_type
    if build_type.lower() in ("debugfast", "debug-fast"):
        display_name = "Debug (fast)"
    display_name += f" ({os_display_name}"
    if cc:
        display_name += f": {os.path.basename(cc)}"
    display_name += ")"
    if sanitizer:
        display_name += f" with {sanitizer}"
    return display_name


def inject_clang_toolchain_flags(
    cxx: str,
    compiler_info: Dict[str, str],
    sanitizer: str = "",
) -> Tuple[Dict[str, str], str, str]:
    """
    For clang/LLVM toolchains, prefer colocated binutils/linker/libc++ if available.

    Works for Homebrew or any LLVM install that keeps tools together.

    Args:
        cxx: C++ compiler path.
        compiler_info: Dictionary with compiler info.
        sanitizer: Sanitizer name if any.

    Returns:
        Tuple of (extra_cache_vars, cc_flags, cxx_flags).
    """
    extra_vars: Dict[str, str] = {}
    cc_flags = ""
    cxx_flags = ""

    compiler_id = compiler_info.get("CMAKE_CXX_COMPILER_ID", "").lower()
    if compiler_id not in ("clang", "appleclang"):
        return extra_vars, cc_flags, cxx_flags

    cxx_path = cxx or compiler_info.get("CMAKE_CXX_COMPILER", "")
    if not cxx_path:
        return extra_vars, cc_flags, cxx_flags

    tool_root = os.path.abspath(os.path.join(os.path.dirname(cxx_path), os.pardir))
    bin_dir = os.path.join(tool_root, "bin")

    # Check for LLVM tools
    for var, tool in [
        ("CMAKE_AR", "llvm-ar"),
        ("CMAKE_CXX_COMPILER_AR", "llvm-ar"),
        ("CMAKE_C_COMPILER_AR", "llvm-ar"),
        ("CMAKE_RANLIB", "llvm-ranlib"),
    ]:
        tool_path = os.path.join(bin_dir, tool)
        if is_executable(tool_path):
            extra_vars[var] = tool_path

    # Check for lld linker
    for linker in ["ld.lld", "lld"]:
        ld_path = os.path.join(bin_dir, linker)
        if is_executable(ld_path):
            extra_vars["CMAKE_C_COMPILER_LINKER"] = ld_path
            extra_vars["CMAKE_CXX_COMPILER_LINKER"] = ld_path
            break

    # Check for libc++
    libcxx_include = os.path.join(tool_root, "include", "c++", "v1")
    libcxx_lib = os.path.join(tool_root, "lib", "c++")
    libunwind = os.path.join(tool_root, "lib", "unwind")

    if os.path.exists(libcxx_include) and os.path.exists(libcxx_lib):
        san_name = sanitizer_flag_name(sanitizer) if sanitizer else ""
        if san_name in ("address", "memory"):
            # Sanitizer builds: use explicit flags to ensure the instrumented
            # libc++ is used instead of the system default.
            cxx_flags += f" -nostdinc++ -nostdlib++ -isystem {libcxx_include}"
            ld_flags = f"-L{libcxx_lib} -lc++ -lc++abi -Wl,-rpath,{libcxx_lib}"
        else:
            cxx_flags += f" -stdlib=libc++ -I{libcxx_include}"
            ld_flags = f"-L{libcxx_lib}"
        if os.path.exists(libunwind):
            ld_flags += f" -L{libunwind} -lunwind"
        if sanitizer:
            flag_name = sanitizer_flag_name(sanitizer)
            ld_flags += f" -fsanitize={flag_name}"
        for var in ["CMAKE_EXE_LINKER_FLAGS", "CMAKE_SHARED_LINKER_FLAGS", "CMAKE_MODULE_LINKER_FLAGS"]:
            extra_vars[var] = ld_flags

    return extra_vars, cc_flags.strip(), cxx_flags.strip()


def normalize_preset_value(
    val: str,
    source_dir: str,
    source_dir_parent: str = "",
    home_dir: str = "",
) -> str:
    """
    Normalize paths in preset values to use CMake variables.

    Args:
        val: Value to normalize.
        source_dir: MrDocs source directory.
        source_dir_parent: Parent of source directory.
        home_dir: User home directory.

    Returns:
        Normalized value with CMake variable references.
    """
    if not isinstance(val, str):
        return val

    parts = val.split(";")
    out_parts = []
    for part in parts:
        p = part
        if source_dir and p.startswith(source_dir):
            p = "${sourceDir}" + p[len(source_dir):]
        elif source_dir_parent and p.startswith(source_dir_parent):
            p = "${sourceParentDir}" + p[len(source_dir_parent):]
        elif home_dir and p.startswith(home_dir):
            p = "$env{HOME}" + p[len(home_dir):]
        out_parts.append(p)
    return ";".join(out_parts)


def create_cmake_presets(
    source_dir: str,
    preset_name: str,
    build_type: str,
    cc: str = "",
    cxx: str = "",
    ninja_path: str = "",
    python_path: str = "",
    git_path: str = "",
    sanitizer: str = "",
    package_roots: Optional[Dict[str, str]] = None,
    compiler_info: Optional[Dict[str, str]] = None,
    valid_package_root_vars: Optional[List[str]] = None,
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
) -> Dict[str, Any]:
    """
    Create or update CMakeUserPresets.json.

    Args:
        source_dir: MrDocs source directory.
        preset_name: Name for the preset.
        build_type: Build type (Debug, Release, etc.).
        cc: C compiler path.
        cxx: C++ compiler path.
        ninja_path: Ninja executable path.
        python_path: Python executable path.
        git_path: Git executable path.
        sanitizer: Sanitizer to use.
        package_roots: Dictionary of package root variables.
        compiler_info: Dictionary of compiler information.
        valid_package_root_vars: List of valid package root variable names from
            current recipes. When provided, stale _ROOT variables not in this
            list are removed from all presets.
        cflags: Extra C compiler flags.
        cxxflags: Extra C++ compiler flags.
        ldflags: Extra linker flags.
        dry_run: If True, only return the preset without writing.
        ui: TextUI instance for output.

    Returns:
        The generated preset dictionary.
    """
    if ui is None:
        ui = get_default_ui()

    package_roots = package_roots or {}
    compiler_info = compiler_info or {}

    user_presets_path = os.path.join(source_dir, "CMakeUserPresets.json")
    if os.path.exists(user_presets_path):
        with open(user_presets_path, "r") as f:
            user_presets = json.load(f)
    else:
        user_presets = {
            "version": 6,
            "cmakeMinimumRequired": {"major": 3, "minor": 21, "patch": 0},
            "configurePresets": []
        }

    host_system_name, os_display_name = get_host_system_name()
    parent_preset = get_parent_preset_name(build_type)
    display_name = get_display_name(build_type, os_display_name, cc, sanitizer)

    # Determine generator
    generator = "Unix Makefiles" if not is_windows() else "Visual Studio 17 2022"
    if ninja_path:
        generator = "Ninja"
    elif "CMAKE_GENERATOR" in compiler_info:
        generator = compiler_info["CMAKE_GENERATOR"]

    main_cmake_build_type = "Debug" if build_type.lower() in ("debugfast", "debug-fast") else build_type

    cache_vars: Dict[str, Any] = {
        "CMAKE_BUILD_TYPE": main_cmake_build_type,
        "MRDOCS_BUILD_DOCS": False,
        "MRDOCS_GENERATE_REFERENCE": False,
        "MRDOCS_GENERATE_ANTORA_REFERENCE": False
    }

    # Add package roots
    for var, path in package_roots.items():
        cache_vars[var] = path

    new_preset = {
        "name": preset_name,
        "generator": generator,
        "displayName": display_name,
        "description": f"Preset for building MrDocs in {build_type} mode with the {os.path.basename(cc) if cc else 'default'} compiler in {os_display_name}.",
        "inherits": parent_preset,
        "binaryDir": "${sourceDir}/build/${presetName}",
        "cacheVariables": cache_vars,
        "warnings": {"unusedCli": False},
        "condition": {
            "type": "equals",
            "lhs": "${hostSystemName}",
            "rhs": host_system_name
        }
    }

    if generator.startswith("Visual Studio"):
        new_preset["architecture"] = "x64"

    if cc:
        new_preset["cacheVariables"]["CMAKE_C_COMPILER"] = cc
    if cxx:
        new_preset["cacheVariables"]["CMAKE_CXX_COMPILER"] = cxx
    if ninja_path:
        new_preset["cacheVariables"]["CMAKE_MAKE_PROGRAM"] = ninja_path
        new_preset["generator"] = "Ninja"

    # Handle sanitizer flags
    cc_flags = ''
    cxx_flags = ''
    ld_flags = ''
    if sanitizer:
        flag_name = sanitizer_flag_name(sanitizer)
        cc_flags = f"-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer"
        cxx_flags = f"-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer"

    # Append user-provided flags
    if cflags:
        cc_flags = (cc_flags + " " + cflags).strip()
    if cxxflags:
        cxx_flags = (cxx_flags + " " + cxxflags).strip()
    if ldflags:
        ld_flags = (ld_flags + " " + ldflags).strip()

    # Inject clang toolchain flags if using clang/LLVM
    extra_cache_vars, extra_cc_flags, extra_cxx_flags = inject_clang_toolchain_flags(
        cxx, compiler_info, sanitizer
    )
    if extra_cc_flags:
        cc_flags = (cc_flags + " " + extra_cc_flags).strip()
    if extra_cxx_flags:
        cxx_flags = (cxx_flags + " " + extra_cxx_flags).strip()
    # Merge user ldflags with injected toolchain linker flags
    for ld_var in ["CMAKE_EXE_LINKER_FLAGS", "CMAKE_SHARED_LINKER_FLAGS", "CMAKE_MODULE_LINKER_FLAGS"]:
        injected = extra_cache_vars.pop(ld_var, "")
        if injected or ld_flags:
            new_preset["cacheVariables"][ld_var] = (injected + " " + ld_flags).strip()
    for var, val in extra_cache_vars.items():
        new_preset["cacheVariables"][var] = val

    if cc_flags:
        new_preset["cacheVariables"]["CMAKE_C_FLAGS"] = cc_flags.strip()
    if cxx_flags:
        new_preset["cacheVariables"]["CMAKE_CXX_FLAGS"] = cxx_flags.strip()

    # Debug mode with Clang: add hardening flags
    if build_type.lower() == "debug":
        is_clang = False
        if cxx and "clang" in os.path.basename(cxx).lower():
            is_clang = True
        elif "CMAKE_CXX_COMPILER_ID" in compiler_info and compiler_info["CMAKE_CXX_COMPILER_ID"].lower() == "clang":
            is_clang = True
        if is_clang:
            hardening_flag = "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE"
            if "CMAKE_CXX_FLAGS" in new_preset["cacheVariables"]:
                new_preset["cacheVariables"]["CMAKE_CXX_FLAGS"] += " " + hardening_flag
            else:
                new_preset["cacheVariables"]["CMAKE_CXX_FLAGS"] = hardening_flag

    # Windows-specific settings
    if is_windows():
        if python_path:
            new_preset["cacheVariables"]["PYTHON_EXECUTABLE"] = python_path
        if git_path:
            new_preset["cacheVariables"]["GIT_EXECUTABLE"] = git_path
            new_preset["cacheVariables"]["GIT_ROOT"] = os.path.dirname(git_path)
        new_preset["vendor"] = {
            "microsoft.com/VisualStudioSettings/CMake/1.0": {
                "hostOS": ["Windows"],
                "intelliSenseMode": "windows-msvc-x64"
            }
        }

    # Normalize paths
    source_dir_parent = os.path.dirname(source_dir)
    if source_dir_parent == source_dir:
        source_dir_parent = ''
    home_dir = os.path.expanduser("~")

    for key, value in list(new_preset["cacheVariables"].items()):
        if isinstance(value, str):
            new_preset["cacheVariables"][key] = normalize_preset_value(
                value, source_dir, source_dir_parent, home_dir
            )

    # Replace or append preset (full replacement, not merge, so stale
    # keys from a previous run are never carried over).
    preset_exists = False
    for i, preset in enumerate(user_presets.get("configurePresets", [])):
        if preset.get("name") == preset_name:
            preset_exists = True
            user_presets["configurePresets"][i] = new_preset
            break
    if not preset_exists:
        user_presets.setdefault("configurePresets", []).append(new_preset)

    # Clean stale package root variables from all presets.
    # Tool-path roots (GIT_ROOT) are preserved; only dependency roots are cleaned.
    _TOOL_ROOT_VARS = {"GIT_ROOT"}
    if valid_package_root_vars is not None:
        valid_roots = set(valid_package_root_vars)
        # Only clean stale _ROOT vars from the preset we just created,
        # not from other user-managed presets in the same file.
        cache = new_preset.get("cacheVariables", {})
        stale_keys = [
            k for k in cache
            if k.endswith("_ROOT")
            and k not in valid_roots
            and k not in _TOOL_ROOT_VARS
        ]
        for k in stale_keys:
            del cache[k]

    # Write file
    write_text(
        user_presets_path,
        json.dumps(user_presets, indent=2),
        dry_run=dry_run,
        ui=ui
    )

    return new_preset
