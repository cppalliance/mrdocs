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
CLI entry point for the MrDocs Bootstrap Tool.

Usage:
    python -m util.bootstrap [options]
    python bootstrap.py [options]
"""

import argparse
import sys

from . import __version__, TRANSITION_BANNER
from .core import (
    TextUI,
    InstallOptions,
    BUILD_TYPES,
    SANITIZERS,
    get_source_dir,
)
from .installer import MrDocsInstaller


def build_arg_parser() -> argparse.ArgumentParser:
    """Build the command-line argument parser."""
    parser = argparse.ArgumentParser(
        prog="bootstrap",
        description="MrDocs Bootstrap Tool - Set up the development environment",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
Examples:
    python bootstrap.py                     # Interactive setup
    python bootstrap.py --yes               # Non-interactive with defaults
    python bootstrap.py --build-type Debug  # Debug build
    python bootstrap.py --clean             # Clean and rebuild dependencies

{TRANSITION_BANNER}
""",
    )

    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )

    # Build configuration
    build_group = parser.add_argument_group("Build Configuration")
    build_group.add_argument(
        "--build-type",
        choices=BUILD_TYPES,
        default=None,
        help="CMake build type (default: Release)",
    )
    build_group.add_argument(
        "--preset",
        default=None,
        help="CMake preset name",
    )
    build_group.add_argument(
        "--sanitizer",
        choices=[s for s in SANITIZERS if s],
        default=None,
        help="Enable sanitizer",
    )
    build_group.add_argument(
        "--build-tests",
        action="store_true",
        default=None,
        help="Build tests",
    )
    build_group.add_argument(
        "--no-build-tests",
        action="store_false",
        dest="build_tests",
        help="Don't build tests",
    )

    # Compiler options
    compiler_group = parser.add_argument_group("Compiler Options")
    compiler_group.add_argument(
        "--cc",
        default=None,
        help="C compiler path",
    )
    compiler_group.add_argument(
        "--cxx",
        default=None,
        help="C++ compiler path",
    )

    # Tool paths
    tools_group = parser.add_argument_group("Tool Paths")
    tools_group.add_argument(
        "--cmake-path",
        default=None,
        help="CMake executable path",
    )
    tools_group.add_argument(
        "--ninja-path",
        default=None,
        help="Ninja executable path",
    )
    tools_group.add_argument(
        "--git-path",
        default=None,
        help="Git executable path",
    )
    tools_group.add_argument(
        "--python-path",
        default=None,
        help="Python executable path",
    )
    tools_group.add_argument(
        "--java-path",
        default=None,
        help="Java executable path",
    )

    # Directories
    dir_group = parser.add_argument_group("Directories")
    dir_group.add_argument(
        "--source-dir",
        default=None,
        help="MrDocs source directory",
    )
    dir_group.add_argument(
        "--build-dir",
        default=None,
        help="Build directory",
    )
    dir_group.add_argument(
        "--install-dir",
        default=None,
        help="Installation directory",
    )

    # Behavior options
    behavior_group = parser.add_argument_group("Behavior Options")
    behavior_group.add_argument(
        "-y", "--yes",
        action="store_true",
        dest="non_interactive",
        help="Non-interactive mode (accept defaults)",
    )
    behavior_group.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without executing",
    )
    behavior_group.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output",
    )
    behavior_group.add_argument(
        "--debug",
        action="store_true",
        help="Debug mode (show tracebacks)",
    )
    behavior_group.add_argument(
        "--plain",
        action="store_true",
        dest="plain_ui",
        help="Plain output (no colors or emojis)",
    )

    # Dependency options
    dep_group = parser.add_argument_group("Dependency Options")
    dep_group.add_argument(
        "--clean",
        action="store_true",
        help="Clean and rebuild all dependencies",
    )
    dep_group.add_argument(
        "--force",
        action="store_true",
        help="Force rebuild even if up to date",
    )
    dep_group.add_argument(
        "--recipe-filter",
        default=None,
        help="Only build specified recipes (comma-separated)",
    )
    dep_group.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip the MrDocs build step",
    )
    dep_group.add_argument(
        "--list-recipes",
        action="store_true",
        help="List available recipes and exit",
    )
    dep_group.add_argument(
        "--refresh-all",
        action="store_true",
        help="Re-run bootstrap for all existing IDE configurations",
    )

    # Run configuration options
    config_group = parser.add_argument_group("Run Configuration Options")
    config_group.add_argument(
        "--generate-run-configs",
        action="store_true",
        default=None,
        help="Generate IDE run configurations",
    )
    config_group.add_argument(
        "--no-run-configs",
        action="store_false",
        dest="generate_run_configs",
        help="Don't generate IDE run configurations",
    )

    return parser


def get_command_line_args(argv=None) -> dict:
    """Parse command-line arguments and return as a dictionary."""
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    # Convert to dictionary, keeping only non-None values
    result = {}
    for key, value in vars(args).items():
        if value is not None:
            # Convert hyphens to underscores for option names
            key = key.replace("-", "_")
            result[key] = value

    return result


def main() -> int:
    """Main entry point."""
    try:
        cmd_args = get_command_line_args()

        installer = MrDocsInstaller(cmd_args)

        # Show transition warning
        installer.ui.warn(TRANSITION_BANNER)

        if cmd_args.get("list_recipes"):
            installer.list_recipes()
            return 0

        if cmd_args.get("refresh_all"):
            installer.refresh_all()
            return 0

        installer.run()
        return 0

    except KeyboardInterrupt:
        print("\nInterrupted by user.")
        return 130
    except Exception as e:
        if cmd_args.get("debug"):
            raise
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
