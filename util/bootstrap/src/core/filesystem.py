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
Filesystem utilities for the bootstrap tool.

Provides functions for common filesystem operations with optional
dry-run support.
"""

import os
import shutil
from typing import Optional

from .ui import TextUI, get_default_ui


def ensure_dir(path: str, exist_ok: bool = True, dry_run: bool = False, ui: Optional[TextUI] = None):
    """
    Create a directory and any necessary parent directories.

    Args:
        path: Directory path to create.
        exist_ok: If True, don't raise an error if the directory exists.
        dry_run: If True, only print what would be done without actually creating.
        ui: TextUI instance for output. Uses default if not provided.
    """
    if ui is None:
        ui = get_default_ui()
    if dry_run:
        ui.info(f"dry-run: would create directory {path}")
        return
    os.makedirs(path, exist_ok=exist_ok)


def remove_dir(path: str, dry_run: bool = False, ui: Optional[TextUI] = None):
    """
    Remove a directory and all its contents.

    Args:
        path: Directory path to remove.
        dry_run: If True, only print what would be done without actually removing.
        ui: TextUI instance for output. Uses default if not provided.
    """
    if ui is None:
        ui = get_default_ui()
    if not os.path.exists(path):
        return
    if dry_run:
        ui.info(f"dry-run: would remove directory {path}")
        return
    shutil.rmtree(path, ignore_errors=True)


def write_text(path: str, content: str, encoding: str = "utf-8", dry_run: bool = False, ui: Optional[TextUI] = None):
    """
    Write text content to a file, creating parent directories if needed.

    Args:
        path: File path to write.
        content: Text content to write.
        encoding: File encoding (default: utf-8).
        dry_run: If True, only print what would be done without actually writing.
        ui: TextUI instance for output. Uses default if not provided.
    """
    if ui is None:
        ui = get_default_ui()
    if dry_run:
        ui.info(f"dry-run: would write file {path}")
        return
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding=encoding) as f:
        f.write(content)


def is_executable(path: str) -> bool:
    """
    Check if a file exists and is executable.

    Args:
        path: File path to check.

    Returns:
        True if the file exists and is executable.
    """
    if not path or not os.path.isfile(path):
        return False
    return os.access(path, os.X_OK)


def is_non_empty_dir(path: str) -> bool:
    """
    Check if a path is a directory and contains at least one entry.

    Args:
        path: Directory path to check.

    Returns:
        True if the path is a non-empty directory.
    """
    if not os.path.isdir(path):
        return False
    try:
        return bool(os.listdir(path))
    except OSError:
        return False


def load_json_file(path: str) -> Optional[dict]:
    """
    Load and parse a JSON file.

    Args:
        path: Path to the JSON file.

    Returns:
        Parsed JSON data as a dict, or None if the file doesn't exist or can't be parsed.
    """
    import json
    if not os.path.isfile(path):
        return None
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return None
