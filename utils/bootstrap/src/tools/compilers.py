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
Compiler detection and probing utilities.

Provides functions to detect compilers and probe their capabilities
using CMake.
"""

import os
import shlex
import shutil
import subprocess
from typing import Optional, Dict

from ..core.platform import is_windows
from ..core.filesystem import ensure_dir, remove_dir, write_text
from ..core.ui import TextUI, get_default_ui
from .detection import is_tool_executable


def check_compiler(
    compiler_path: str,
    compiler_type: str = "cc",
) -> str:
    """
    Validate and resolve a compiler path.

    Args:
        compiler_path: Path to the compiler (can be relative or just a name).
        compiler_type: Type of compiler ('cc' or 'cxx').

    Returns:
        Absolute path to the compiler executable.

    Raises:
        FileNotFoundError: If the compiler is not found.
    """
    if not compiler_path:
        return ""

    if not os.path.isabs(compiler_path):
        resolved = shutil.which(compiler_path)
        if resolved is None:
            raise FileNotFoundError(
                f"{compiler_type} executable '{compiler_path}' not found in PATH."
            )
        compiler_path = resolved

    if not is_tool_executable(compiler_path):
        raise FileNotFoundError(
            f"{compiler_type} executable not found at {compiler_path}."
        )

    return compiler_path


def probe_compilers(
    cmake_path: str,
    probe_dir: str,
    cc: Optional[str] = None,
    cxx: Optional[str] = None,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
) -> Dict[str, str]:
    """
    Probe compiler information using CMake.

    Creates a minimal CMake project to extract compiler details like
    compiler ID, version, and paths.

    Args:
        cmake_path: Path to CMake executable.
        probe_dir: Directory to use for the probe project.
        cc: Optional C compiler path.
        cxx: Optional C++ compiler path.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.

    Returns:
        Dictionary mapping CMAKE_* variables to their values.

    Raises:
        RuntimeError: If CMake probe fails.
    """
    if ui is None:
        ui = get_default_ui()

    ui.info("Probing default system compilers...")

    variables = []
    for lang in ["C", "CXX"]:
        for suffix in [
            "COMPILER", "COMPILER_ID", "COMPILER_VERSION",
            "COMPILER_AR", "COMPILER_LINKER", "COMPILER_LINKER_ID",
            "COMPILER_ABI"
        ]:
            variables.append(f"CMAKE_{lang}_{suffix}")
    variables.append("CMAKE_GENERATOR")

    # Build the CMakeLists.txt content for probing
    cmake_lines = [
        "cmake_minimum_required(VERSION 3.10)",
        "project(probe C CXX)"
    ]
    for var in variables:
        cmake_lines.append(f'message(STATUS "{var}=${{{var}}}")')
    cmake_content = "\n".join(cmake_lines)

    if dry_run:
        ensure_dir(probe_dir, dry_run=True, ui=ui)
        write_text(os.path.join(probe_dir, "CMakeLists.txt"), cmake_content, dry_run=True, ui=ui)
        from ..core.process import run_cmd as _run_cmd
        cmd = [cmake_path, "-S", probe_dir]
        if cc:
            cmd.append(f"-DCMAKE_C_COMPILER={cc}")
        if cxx:
            cmd.append(f"-DCMAKE_CXX_COMPILER={cxx}")
        cmd.extend(["-B", os.path.join(probe_dir, "build")])
        _run_cmd(cmd, dry_run=True, ui=ui)
        remove_dir(probe_dir, dry_run=True, ui=ui)
        return {}

    # Clean up any existing probe directory
    if os.path.exists(probe_dir):
        remove_dir(probe_dir, dry_run=False, ui=ui)
    ensure_dir(probe_dir, dry_run=False, ui=ui)

    # Create minimal CMakeLists.txt
    write_text(
        os.path.join(probe_dir, "CMakeLists.txt"),
        cmake_content,
        dry_run=False,
        ui=ui
    )

    # Build CMake command
    cmd = [cmake_path, "-S", probe_dir]
    env = os.environ.copy()
    if cc:
        cmd.append(f"-DCMAKE_C_COMPILER={cc}")
    if cxx:
        cmd.append(f"-DCMAKE_CXX_COMPILER={cxx}")
    cmd.extend(["-B", os.path.join(probe_dir, "build")])

    # Run cmake and capture output
    result = subprocess.run(cmd, env=env, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(f"CMake failed:\n{result.stdout}\n{result.stderr}")

    # Parse values from lines like: "-- VAR=value"
    values = {}
    for line in result.stdout.splitlines():
        if line.startswith("-- "):
            for var in variables:
                prefix = f"{var}="
                if prefix in line:
                    values[var] = line.split(prefix, 1)[1].strip()

    # Clean up probe directory
    remove_dir(probe_dir, dry_run=False, ui=ui)

    # Print default C++ compiler info
    compiler_id = values.get('CMAKE_CXX_COMPILER_ID', 'unknown')
    compiler_path = values.get('CMAKE_CXX_COMPILER', 'unknown')
    ui.info(f"Default C++ compiler: {compiler_id} ({compiler_path})")

    return values


def sanitizer_flag_name(sanitizer: str) -> str:
    """
    Get the compiler flag name for a sanitizer.

    Args:
        sanitizer: Sanitizer name (asan, ubsan, msan, tsan, address, undefined, etc.)

    Returns:
        The sanitizer flag name for use with -fsanitize=.
    """
    sanitizer_lower = sanitizer.lower()
    mapping = {
        "asan": "address",
        "ubsan": "undefined",
        "msan": "memory",
        "tsan": "thread",
    }
    return mapping.get(sanitizer_lower, sanitizer_lower)
