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
Archive extraction utilities.

Provides functions to extract zip and tar archives while flattening
the top-level directory.
"""

import os
import shutil
import tarfile
import zipfile
from typing import Optional

from ..core.filesystem import ensure_dir
from ..core.ui import TextUI, get_default_ui


def extract_zip_flatten(
    zip_path: str,
    dest_dir: str,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Extract a zip archive, flattening the top-level directory.

    Many GitHub archives contain a single top-level directory (e.g.,
    "repo-main/"). This function strips that prefix during extraction.

    Args:
        zip_path: Path to the zip file.
        dest_dir: Destination directory for extracted files.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    if dry_run:
        ui.info(f"dry-run: would extract {zip_path} into {dest_dir}")
        return

    with zipfile.ZipFile(zip_path, 'r') as zf:
        infos = zf.infolist()
        # Determine top-level prefix
        prefix = None
        for info in infos:
            name = info.filename
            if name.endswith("/"):
                continue
            parts = name.split("/", 1)
            if len(parts) == 2:
                prefix = parts[0] + "/"
                break
        if prefix is None:
            prefix = ""

        for info in infos:
            name = info.filename
            if name.endswith("/"):
                continue
            rel = name[len(prefix):] if name.startswith(prefix) else name
            target_path = os.path.join(dest_dir, rel)
            target_dir = os.path.dirname(target_path)
            ensure_dir(target_dir, dry_run=False, ui=ui)
            with zf.open(info, 'r') as src, open(target_path, 'wb') as dst:
                shutil.copyfileobj(src, dst)


def extract_tar_flatten(
    tar_path: str,
    dest_dir: str,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Extract a tar archive, flattening the top-level directory.

    Many GitHub archives contain a single top-level directory (e.g.,
    "repo-main/"). This function strips that prefix during extraction.

    Args:
        tar_path: Path to the tar file.
        dest_dir: Destination directory for extracted files.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    if dry_run:
        ui.info(f"dry-run: would extract {tar_path} into {dest_dir}")
        return

    mode = "r:*"
    with tarfile.open(tar_path, mode) as tf:
        # Determine top-level prefix
        prefix = None
        for member in tf.getmembers():
            parts = member.name.split("/", 1)
            if len(parts) == 2:
                prefix = parts[0] + "/"
                break
        if prefix is None:
            prefix = ""

        for member in tf.getmembers():
            if member.isdir():
                continue
            rel = member.name[len(prefix):] if member.name.startswith(prefix) else member.name
            target_path = os.path.join(dest_dir, rel)
            ensure_dir(os.path.dirname(target_path), dry_run=False, ui=ui)
            with tf.extractfile(member) as src, open(target_path, "wb") as dst:
                shutil.copyfileobj(src, dst)
