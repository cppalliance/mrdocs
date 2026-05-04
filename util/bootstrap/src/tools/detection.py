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
Generic tool detection utilities.

Provides functions to find executables in PATH, environment variables,
and standard installation locations.
"""

import os
import shutil
import sys
from typing import Optional

from ..core.platform import is_windows


def is_tool_executable(path: str) -> bool:
    """
    Check if a path points to an executable file.

    Args:
        path: Path to check.

    Returns:
        True if the path is an executable file.
    """
    if not os.path.exists(path):
        return False
    if not os.path.isfile(path):
        return False
    if is_windows():
        # On Windows, check for executable extensions
        _, ext = os.path.splitext(path)
        return ext.lower() in [".exe", ".bat", ".cmd", ".com"]
    else:
        return os.access(path, os.X_OK)


def find_tool(tool: str) -> Optional[str]:
    """
    Find a tool executable using environment variables and PATH.

    Searches in order:
    1. Environment variables like TOOL_ROOT, TOOL_DIR, TOOL_PATH, etc.
    2. System PATH
    3. Visual Studio installation (Windows only)
    4. Special handling for python

    Args:
        tool: Name of the tool to find.

    Returns:
        Path to the tool executable, or None if not found.
    """
    # 1. Check environment variables
    env_suffixes = ["ROOT", "DIR", "PATH", "HOME", "INSTALL_DIR", "EXECUTABLE"]
    env_prefixes = [tool.upper(), tool.lower(), tool.title()]

    for env_prefix in env_prefixes:
        for env_suffix in env_suffixes:
            env_var = f"{env_prefix}_{env_suffix}"
            env_path = os.environ.get(env_var)
            if env_path and os.path.exists(env_path):
                if is_tool_executable(env_path):
                    return env_path
                if os.path.isdir(env_path):
                    tool_filename = tool if tool.endswith(".exe") else (tool + ".exe" if is_windows() else tool)
                    tool_path = os.path.join(env_path, tool_filename)
                    if is_tool_executable(tool_path):
                        return tool_path
                    tool_bin_path = os.path.join(env_path, 'bin', tool_filename)
                    if is_tool_executable(tool_bin_path):
                        return tool_bin_path

    # 2. Look for the tool in system PATH
    tool_path = shutil.which(tool)
    if tool_path:
        return tool_path

    # 3. Windows-specific: check Visual Studio installation
    if is_windows():
        from .visual_studio import find_vs_tool
        tool_path = find_vs_tool(tool)
        if tool_path:
            return tool_path

    # 4. Special handling for python
    if tool == "python":
        return sys.executable

    return None

