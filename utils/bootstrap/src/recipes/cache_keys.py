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
Cache key generation for CI-compatible dependency caching.

Generates cache keys matching the existing CI format so switching
to bootstrap doesn't invalidate caches.
"""

import os
import re
import subprocess
from typing import Tuple

from ..tools.compilers import sanitizer_flag_name


def generate_cache_key(
    recipe_name: str,
    recipe_hash: str,
    build_type: str,
    os_key: str,
    compiler: str = "",
    compiler_version: str = "",
    sanitizer: str = "",
) -> str:
    """
    Generate a CI-compatible cache key for a recipe.

    Format: {name}-{7-char-hash}-{build-type-lower}-{os-key}
    with optional -{compiler}-{version}-{sanitizer} suffix.

    The optional suffix is added only for clang with address or memory
    sanitizers, matching the existing CI Handlebars template logic.

    Colons in os-key are replaced with hyphens (e.g. ubuntu:24.04
    becomes ubuntu-24.04).

    Args:
        recipe_name: Recipe name (e.g. "llvm").
        recipe_hash: Full commit hash from the recipe version field.
        build_type: CMake build type (e.g. "Release").
        os_key: OS/container identifier (e.g. "ubuntu:24.04", "macos-15").
        compiler: Compiler family name (e.g. "clang", "gcc").
        compiler_version: Compiler major version (e.g. "19").
        sanitizer: Sanitizer name (e.g. "address", "asan", "memory").

    Returns:
        Cache key string.
    """
    short_hash = recipe_hash[:7]
    bt = build_type.lower()
    os_normalized = os_key.replace(":", "-")

    key = f"{recipe_name}-{short_hash}-{bt}-{os_normalized}"

    # Add suffix only for clang + (asan or msan), matching CI template
    san = sanitizer_flag_name(sanitizer.lower()) if sanitizer else ""
    if compiler.lower() == "clang" and san in ("address", "memory"):
        sanitizer_str = _sanitizer_archive_str(san)
        key += f"-{compiler}-{compiler_version}-{sanitizer_str}"

    return key


def _sanitizer_archive_str(sanitizer: str) -> str:
    """
    Map normalized sanitizer name to CI archive string.

    Matches the CI Handlebars template llvm-archive-sanitizer-str:
    address -> ASan, undefined -> UBSan.
    """
    mapping = {
        "address": "ASan",
        "undefined": "UBSan",
    }
    return mapping.get(sanitizer, "")


def detect_compiler_for_cache_key(cc: str) -> Tuple[str, str]:
    """
    Detect compiler family and major version from a compiler path.

    Uses the path basename to identify the compiler family, then
    extracts the version from the path or by running the compiler.

    Args:
        cc: Path to the C compiler (e.g. "/usr/bin/clang-19", "gcc-14").

    Returns:
        Tuple of (compiler_name, major_version).
        compiler_name is lowercase (e.g. "clang", "gcc").
        major_version is just the major number (e.g. "19").
    """
    if not cc:
        return ("", "")

    basename = os.path.basename(cc).lower()

    # Determine compiler family
    if "clang" in basename:
        name = "clang"
    elif "gcc" in basename or "g++" in basename:
        name = "gcc"
    elif basename in ("cl", "cl.exe"):
        name = "msvc"
    else:
        name = ""

    # Try to extract version from path name (e.g. clang-19, gcc-14)
    m = re.search(r'[-](\d+)', basename)
    if m:
        return (name, m.group(1))

    # Try running the compiler to get version
    try:
        result = subprocess.run(
            [cc, "-dumpversion"], capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            version = result.stdout.strip().split(".")[0]
            return (name, version)
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        pass

    return (name, "")
