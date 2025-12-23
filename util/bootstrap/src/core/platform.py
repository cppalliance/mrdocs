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
Platform detection utilities.

Provides functions for detecting the current operating system and
determining if the script is running from the MrDocs source directory.
"""

import os
import sys
from functools import lru_cache


@lru_cache(maxsize=1)
def running_from_mrdocs_source_dir() -> bool:
    """
    Check if the current working directory is the MrDocs source directory.

    This is determined by checking if CWD matches the directory containing
    the bootstrap module (two levels up from this file).

    Returns:
        True if running from the MrDocs source directory.
    """
    # The source dir is the mrdocs root, which is 4 levels up from this file:
    # util/bootstrap/src/core/platform.py -> mrdocs/
    this_file = os.path.abspath(__file__)
    source_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(this_file)))))
    cwd = os.getcwd()
    return cwd == source_dir


@lru_cache(maxsize=1)
def get_source_dir() -> str:
    """
    Get the MrDocs source directory path.

    Returns:
        Absolute path to the MrDocs source directory.
    """
    this_file = os.path.abspath(__file__)
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(this_file)))))

@lru_cache(maxsize=1)
def is_windows() -> bool:
    """
    Check if the current platform is Windows.

    Returns:
        True if running on Windows.
    """
    return os.name == "nt"

@lru_cache(maxsize=1)
def is_linux() -> bool:
    """
    Check if the current platform is Linux.

    Returns:
        True if running on Linux.
    """
    return sys.platform.startswith("linux")

@lru_cache(maxsize=1)
def is_macos() -> bool:
    """
    Check if the current platform is macOS.

    Returns:
        True if running on macOS.
    """
    return sys.platform == "darwin"

@lru_cache(maxsize=1)
def get_os_name() -> str:
    """
    Get a lowercase OS name suitable for use in paths and presets.

    Returns:
        'windows', 'linux', or 'macos'.
    """
    if is_windows():
        return "windows"
    elif is_linux():
        return "linux"
    elif is_macos():
        return "macos"
    else:
        return sys.platform


def supports_ansi() -> bool:
    """
    Check if the terminal supports ANSI escape codes.

    Returns:
        True if ANSI codes are likely supported.
    """
    if is_windows():
        # Windows 10+ supports ANSI in cmd/powershell with VT mode
        return os.environ.get("TERM") is not None or os.environ.get("WT_SESSION") is not None
    return True
