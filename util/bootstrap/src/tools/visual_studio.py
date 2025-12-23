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
Visual Studio tool detection (Windows-specific).

Provides functions to find tools bundled with Visual Studio installations.
"""

import json
import os
import subprocess
from functools import lru_cache
from typing import Optional, List

from ..core.platform import is_windows
from .detection import is_tool_executable


@lru_cache(maxsize=1)
def get_vs_install_locations() -> Optional[List[str]]:
    """
    Get Visual Studio installation locations using vswhere.

    Returns:
        List of VS installation paths, or None if vswhere is not available.
    """
    if not is_windows():
        return []

    p = os.environ.get('ProgramFiles(x86)', r"C:\Program Files (x86)")
    path_vswhere = os.path.join(
        p,
        "Microsoft Visual Studio",
        "Installer",
        "vswhere.exe"
    )

    if not is_tool_executable(path_vswhere):
        return None

    cmd = [
        path_vswhere,
        "-latest",
        "-products", "*",
        "-requires", "Microsoft.Component.MSBuild",
        "-format", "json"
    ]

    try:
        data = subprocess.check_output(cmd, universal_newlines=True)
        info = json.loads(data)
        if not info:
            return None
        return [inst.get("installationPath") for inst in info]
    except (subprocess.CalledProcessError, json.JSONDecodeError):
        return None


def find_vs_tool(tool: str) -> Optional[str]:
    """
    Find a tool bundled with Visual Studio.

    Supported tools: cmake, ninja, git

    Args:
        tool: Name of the tool to find.

    Returns:
        Path to the tool executable, or None if not found.
    """
    if not is_windows():
        return None

    vs_tools = ["cmake", "ninja", "git", "python"]
    if tool not in vs_tools:
        return None

    vs_roots = get_vs_install_locations()
    if not vs_roots:
        return None

    for vs_root in vs_roots:
        ms_cext_path = os.path.join(
            vs_root,
            "Common7",
            "IDE",
            "CommonExtensions",
            "Microsoft"
        )
        toolpaths = {
            'cmake': os.path.join(ms_cext_path, "CMake", "CMake", "bin", "cmake.exe"),
            'git': os.path.join(
                ms_cext_path,
                "TeamFoundation",
                "Team Explorer",
                "Git",
                "cmd",
                "git.exe"
            ),
            'ninja': os.path.join(ms_cext_path, "CMake", "Ninja", "ninja.exe")
        }
        path = toolpaths.get(tool)
        if path and is_tool_executable(path):
            return path

    return None


@lru_cache(maxsize=1)
def probe_msvc_dev_env() -> Optional[dict]:
    """
    Probe MSVC development environment variables by running vcvarsall.bat.

    This extracts the environment variables set by Visual Studio's developer
    command prompt, which are needed for MSVC builds.

    Returns:
        Dictionary of environment variables to add/update, or None if not on Windows
        or vcvarsall.bat is not found.
    """
    if not is_windows():
        return None

    print("Probing MSVC development environment variables...")
    vs_roots = get_vs_install_locations()
    vcvarsall_path = None
    for vs_root in vs_roots or []:
        vcvarsall_path_candidate = os.path.join(vs_root, "VC", "Auxiliary", "Build", "vcvarsall.bat")
        if os.path.exists(vcvarsall_path_candidate):
            vcvarsall_path = vcvarsall_path_candidate
            print(f"Found vcvarsall.bat at {vcvarsall_path}.")
            break

    if not vcvarsall_path:
        print("No vcvarsall.bat found. MSVC development environment variables will not be set.")
        return None

    # Run vcvarsall.bat with x64 argument and VSCMD_DEBUG=2 to get environment output
    cmd = [vcvarsall_path, "x64"]
    env = os.environ.copy()
    env["VSCMD_DEBUG"] = "2"
    result = subprocess.run(cmd, env=env, text=True, capture_output=True, shell=True)

    if result.returncode != 0:
        print(f"vcvarsall.bat failed: {result.stderr}")
        return None

    # Parse the post-init environment variables from the output
    post_env = {}
    in_post_init_header = False
    for line in result.stdout.splitlines():
        contains_post_init_header = "--------------------- VS Developer Command Prompt Environment [post-init] ---------------------" in line
        if contains_post_init_header:
            if in_post_init_header:
                break
            in_post_init_header = True
            continue
        if not in_post_init_header:
            continue
        if '=' in line:
            key, value = line.split('=', 1)
            post_env[key.strip()] = value.strip()

    if not in_post_init_header or not post_env:
        print("No post-init environment variables found in vcvarsall.bat output.")
        return None

    # Return only the variables that differ from the current environment
    result_env = {}
    current_env = os.environ
    for key, value in post_env.items():
        if key not in current_env or value != current_env[key]:
            result_env[key] = value

    print("MSVC development environment variables extracted successfully.")
    return result_env
