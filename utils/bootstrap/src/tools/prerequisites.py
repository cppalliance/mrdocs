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
System prerequisite detection and reporting.

Checks for required system tools before bootstrap runs and provides
clear, platform-specific install instructions when tools are missing.
"""

import shutil
import subprocess
from dataclasses import dataclass, field
from typing import List, Optional

from ..core.platform import is_windows, is_macos, is_linux
from ..core.ui import TextUI, get_default_ui
from .detection import find_tool, is_tool_executable


@dataclass
class Prerequisite:
    """A system tool that bootstrap requires or optionally uses."""
    name: str
    description: str
    required: bool = True
    install_linux: str = ""
    install_macos: str = ""
    install_windows: str = ""
    check_cmd: str = ""  # Command to verify (e.g. "cmake --version")
    found_path: str = ""


# The canonical list of prerequisites bootstrap needs.
# Ninja is excluded because bootstrap downloads it automatically.
PREREQUISITES = [
    Prerequisite(
        name="cmake",
        description="CMake build system generator",
        required=True,
        install_linux="sudo apt-get install -y cmake",
        install_macos="brew install cmake",
        install_windows="choco install cmake   # or: winget install Kitware.CMake",
        check_cmd="cmake --version",
    ),
    Prerequisite(
        name="git",
        description="Git version control",
        required=True,
        install_linux="sudo apt-get install -y git",
        install_macos="brew install git   # or: xcode-select --install",
        install_windows="choco install git   # or: winget install Git.Git",
        check_cmd="git --version",
    ),
    Prerequisite(
        name="python3",
        description="Python 3 interpreter",
        required=True,
        install_linux="sudo apt-get install -y python3",
        install_macos="brew install python3",
        install_windows="choco install python3   # or: winget install Python.Python.3",
        check_cmd="python3 --version",
    ),
    Prerequisite(
        name="cc",
        description="C compiler (gcc, clang, or MSVC cl)",
        required=True,
        install_linux="sudo apt-get install -y build-essential   # or: sudo apt-get install -y clang",
        install_macos="xcode-select --install",
        install_windows="Install Visual Studio with C++ workload   # or: choco install visualstudio2022-workload-nativedesktop",
        check_cmd="",
    ),
    Prerequisite(
        name="java",
        description="Java runtime (for XML schema validation in tests)",
        required=False,
        install_linux="sudo apt-get install -y default-jre",
        install_macos="brew install openjdk",
        install_windows="choco install openjdk   # or: winget install Microsoft.OpenJDK.21",
        check_cmd="java -version",
    ),
]


def _find_c_compiler() -> Optional[str]:
    """Find any C compiler on the system."""
    candidates = ["cc", "gcc", "clang", "cl"]
    if is_windows():
        candidates = ["cl", "gcc", "clang", "cc"]
    for name in candidates:
        path = shutil.which(name)
        if path:
            return path
    # Also check via find_tool for env-var based discovery
    for name in candidates:
        path = find_tool(name)
        if path:
            return path
    return None


def _find_prerequisite(prereq: Prerequisite) -> Optional[str]:
    """Locate a prerequisite tool and return its path, or None."""
    if prereq.name == "cc":
        return _find_c_compiler()

    if prereq.name == "python3":
        # find_tool("python") already falls back to sys.executable
        path = find_tool("python")
        if path:
            return path
        path = find_tool("python3")
        return path

    return find_tool(prereq.name)


def _get_install_hint(prereq: Prerequisite) -> str:
    """Return a platform-specific install instruction string."""
    if is_linux():
        return prereq.install_linux
    elif is_macos():
        return prereq.install_macos
    elif is_windows():
        return prereq.install_windows
    return prereq.install_linux  # fallback


def check_prerequisites(
    build_tests: bool = True,
    cc: str = "",
    cxx: str = "",
    ui: Optional[TextUI] = None,
) -> List[Prerequisite]:
    """
    Check all prerequisites and return a list of those that are missing.

    Args:
        build_tests: If True, also check optional prerequisites needed for tests (java).
        cc: User-specified C compiler path (if set, skip compiler search).
        cxx: User-specified C++ compiler path (if set, skip compiler search).
        ui: TextUI for output.

    Returns:
        List of missing Prerequisite objects.
    """
    if ui is None:
        ui = get_default_ui()

    missing = []
    import copy
    checked = [copy.copy(p) for p in PREREQUISITES]
    for prereq in checked:
        # Skip optional tools when not needed
        if not prereq.required:
            if prereq.name == "java" and not build_tests:
                continue

        # If user already specified a compiler, skip auto-detection
        if prereq.name == "cc" and (cc or cxx):
            prereq.found_path = cc or cxx
            continue

        path = _find_prerequisite(prereq)
        if path:
            prereq.found_path = path
        else:
            missing.append(prereq)

    return missing


def report_missing_prerequisites(
    missing: List[Prerequisite],
    ui: Optional[TextUI] = None,
):
    """
    Print clear error messages for each missing prerequisite.

    Args:
        missing: List of missing Prerequisite objects.
        ui: TextUI for output.
    """
    if ui is None:
        ui = get_default_ui()

    if not missing:
        return

    required_missing = [p for p in missing if p.required]
    optional_missing = [p for p in missing if not p.required]

    if required_missing:
        ui.error_block(
            "Missing required system tools:",
            [f"{p.name} - {p.description}" for p in required_missing],
        )
        ui.info("")
        ui.info("Install the missing tools:")
        for p in required_missing:
            hint = _get_install_hint(p)
            ui.info(f"  {p.name}: {hint}")

    if optional_missing:
        ui.warn("Missing optional tools (some features may be unavailable):")
        for p in optional_missing:
            hint = _get_install_hint(p)
            ui.info(f"  {p.name} - {p.description}")
            ui.info(f"    Install: {hint}")


def try_install_system_deps(
    missing: List[Prerequisite],
    non_interactive: bool = False,
    ui: Optional[TextUI] = None,
) -> List[Prerequisite]:
    """
    Attempt to install missing prerequisites using the system package manager.

    Only works on Linux (apt-get) and macOS (brew). On Windows, prints
    instructions and returns the still-missing list.

    Args:
        missing: List of missing Prerequisite objects.
        non_interactive: If True, install without prompting.
        ui: TextUI for output.

    Returns:
        List of prerequisites that are still missing after install attempt.
    """
    if ui is None:
        ui = get_default_ui()

    if not missing:
        return []

    pkg_manager = _detect_package_manager()
    if not pkg_manager:
        ui.warn("No supported package manager detected. Please install manually:")
        for p in missing:
            hint = _get_install_hint(p)
            ui.info(f"  {p.name}: {hint}")
        return missing

    # Build install commands grouped by package manager
    packages = _get_package_names(missing, pkg_manager)
    if not packages:
        return missing

    if pkg_manager == "apt-get":
        cmd = ["sudo", "apt-get", "install", "-y"] + packages
    elif pkg_manager == "brew":
        cmd = ["brew", "install"] + packages
    else:
        return missing

    ui.info(f"Installing missing tools: {' '.join(cmd)}")

    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        ui.error(f"Package installation failed (exit code {e.returncode})")
        return missing
    except FileNotFoundError:
        ui.error(f"Package manager '{pkg_manager}' not found")
        return missing

    # Re-check which are still missing
    still_missing = []
    for prereq in missing:
        path = _find_prerequisite(prereq)
        if not path:
            still_missing.append(prereq)
    return still_missing


def _detect_package_manager() -> Optional[str]:
    """Detect the system package manager."""
    if is_linux():
        if shutil.which("apt-get"):
            return "apt-get"
    elif is_macos():
        if shutil.which("brew"):
            return "brew"
    return None


# Maps prerequisite names to actual package names per package manager.
_APT_PACKAGES = {
    "cmake": "cmake",
    "git": "git",
    "python3": "python3",
    "cc": "build-essential",
    "java": "default-jre",
}

_BREW_PACKAGES = {
    "cmake": "cmake",
    "git": "git",
    "python3": "python3",
    "cc": None,  # Xcode command-line tools, not a brew package
    "java": "openjdk",
}


def _get_package_names(
    prereqs: List[Prerequisite],
    pkg_manager: str,
) -> List[str]:
    """Map prerequisites to package names for the given package manager."""
    pkg_map = _APT_PACKAGES if pkg_manager == "apt-get" else _BREW_PACKAGES
    packages = []
    for p in prereqs:
        pkg = pkg_map.get(p.name)
        if pkg:
            packages.append(pkg)
    return packages
