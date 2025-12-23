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
Ninja build system installation utilities.

Provides functions to download and install Ninja if not available.
"""

import json
import os
import platform
import urllib.request
import zipfile
from typing import Optional

from ..core.platform import is_windows
from ..core.filesystem import ensure_dir
from ..core.ui import TextUI, get_default_ui
from .detection import find_tool, is_tool_executable


def get_ninja_asset_name() -> Optional[str]:
    """
    Get the Ninja release asset name for the current platform.

    Returns:
        Asset filename like 'ninja-linux.zip', or None if platform unsupported.
    """
    system = platform.system().lower()
    arch = platform.machine().lower()

    if system == 'linux':
        if arch in ('aarch64', 'arm64'):
            return 'ninja-linux-aarch64.zip'
        else:
            return 'ninja-linux.zip'
    elif system == 'darwin':
        return 'ninja-mac.zip'
    elif system == 'windows':
        if arch in ('arm64', 'aarch64'):
            return 'ninja-winarm64.zip'
        else:
            return 'ninja-win.zip'
    else:
        return None


def install_ninja(
    source_dir: str,
    preset: str,
    ninja_path: Optional[str] = None,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
) -> Optional[str]:
    """
    Install Ninja build system if not available.

    Checks for existing Ninja installation, and downloads from GitHub
    releases if not found.

    Args:
        source_dir: MrDocs source directory.
        preset: Build preset name (used for install location).
        ninja_path: Optional user-specified Ninja path.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.

    Returns:
        Path to Ninja executable, or None if installation failed.
    """
    if ui is None:
        ui = get_default_ui()

    # 1. Check if user specified a path
    if ninja_path:
        if not os.path.isabs(ninja_path):
            ninja_path = find_tool(ninja_path)
        if ninja_path and is_tool_executable(ninja_path):
            return ninja_path
        raise FileNotFoundError(f"Ninja executable not found at {ninja_path}.")

    # 2. Check if ninja is in PATH
    ninja_path = find_tool("ninja")
    if ninja_path:
        ui.info(f"Ninja found in PATH at {ninja_path}. Using it.")
        return ninja_path

    # 3. Download ninja
    tp_root = os.path.join(source_dir, "build", "third-party")
    download_dir = os.path.join(tp_root, "source", "ninja")
    install_dir = os.path.join(tp_root, "install", preset, "ninja")

    ensure_dir(download_dir, dry_run=dry_run, ui=ui)
    ensure_dir(install_dir, dry_run=dry_run, ui=ui)

    exe_name = 'ninja.exe' if is_windows() else 'ninja'
    ninja_exe_path = os.path.join(install_dir, exe_name)

    # Check if already downloaded
    if os.path.exists(ninja_exe_path) and is_tool_executable(ninja_exe_path):
        try:
            rel = os.path.relpath(ninja_exe_path, source_dir)
            display_path = "./" + rel if not rel.startswith("..") else ninja_exe_path
        except Exception:
            display_path = ninja_exe_path
        ui.ok(f"[ninja] already available at {display_path}; reusing.")
        return ninja_exe_path

    # Determine asset name
    asset_name = get_ninja_asset_name()
    if not asset_name:
        return None

    # Fetch download URL from GitHub API
    api_url = 'https://api.github.com/repos/ninja-build/ninja/releases/latest'

    if dry_run:
        ui.info(f"dry-run: would fetch {api_url} and download {asset_name} -> {download_dir}")
        return ninja_exe_path

    print(f"Fetching Ninja release info...")
    with urllib.request.urlopen(api_url) as resp:
        data = json.load(resp)

    release_assets = data.get('assets', [])
    download_url = None
    for asset in release_assets:
        if asset.get('name') == asset_name:
            download_url = asset.get('browser_download_url')
            break

    if not download_url:
        return None

    # Download the asset
    tmpzip = os.path.join(download_dir, asset_name)
    ensure_dir(download_dir, dry_run=False, ui=ui)
    print(f'Downloading {asset_name}...')
    urllib.request.urlretrieve(download_url, tmpzip)

    # Extract
    print('Extracting...')
    ensure_dir(install_dir, dry_run=False, ui=ui)
    with zipfile.ZipFile(tmpzip, 'r') as z:
        z.extractall(install_dir)
    os.remove(tmpzip)

    # Set executable permission on Unix
    if not is_windows():
        os.chmod(ninja_exe_path, 0o755)

    return ninja_exe_path
