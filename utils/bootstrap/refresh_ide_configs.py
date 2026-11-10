#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#
"""Refresh just the IDE run configurations.

Calls `generate_run_configs` directly, without running the full bootstrap
pipeline. Useful when `utils/bootstrap/src/configs/run_configs.py` gains a
new entry and you want it in your IDE without rebuilding dependencies.

The program of every target entry is read from CMake's file API reply in the
preset's build directory. The script writes the query, and CMake answers it the
next time it configures (the top-level CMakeLists.txt writes it too), so a
preset that CMake has not configured since fails with a message naming
`cmake --preset <preset>`.

Usage:

    python3 utils/bootstrap/refresh_ide_configs.py [--preset NAME]

If `--preset` is omitted the script picks the most-recently-modified preset
under `build/`. The saved arguments of the `Bootstrap Refresh (<preset>)` and
`Bootstrap Update (<preset>)` entries are kept as they are (only missing
`--preset`, `--build-dir` and `--install-dir` are added), so a later
`bootstrap.py --refresh-all` still replays the flags of the full run.
"""

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "utils"))

from bootstrap.src.installer import MrDocsInstaller  # noqa: E402
from bootstrap.src.configs import generate_run_configs  # noqa: E402
from bootstrap.src.configs.cmake_file_api import write_query  # noqa: E402
from bootstrap.src.configs.run_configs import load_saved_bootstrap_args  # noqa: E402


def _pick_preset(source_dir: str) -> str:
    """Pick the preset whose build directory was modified most recently."""
    build_root = os.path.join(source_dir, "build")
    if not os.path.isdir(build_root):
        return "release"
    candidates = []
    for name in os.listdir(build_root):
        if name in {"third-party", "site", "site-local", "site-remote"}:
            continue
        path = os.path.join(build_root, name)
        if not os.path.isdir(path):
            continue
        candidates.append((os.path.getmtime(path), name))
    if not candidates:
        return "release"
    candidates.sort(reverse=True)
    return candidates[0][1]


def refresh(installer: MrDocsInstaller) -> None:
    """Regenerate the run configurations, keeping the saved bootstrap arguments."""
    saved_args = load_saved_bootstrap_args(installer.options.source_dir)
    generate_run_configs(
        options=installer.options,
        default_options=installer.default_options,
        compiler_info=installer.compiler_info,
        generate_clion=installer.options.generate_clion_run_configs,
        generate_vscode=installer.options.generate_vscode_run_configs,
        generate_vs=installer.options.generate_vs_run_configs,
        generate_justfile=installer.options.generate_justfile_run_configs,
        dry_run=installer.options.dry_run,
        ui=installer.ui,
        saved_args=saved_args,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--preset",
        help=(
            "Bootstrap preset whose paths the run configs should use. "
            "Defaults to the most recently used build/<preset>/ directory."
        ),
    )
    args = parser.parse_args()

    installer = MrDocsInstaller()
    if args.preset:
        installer.options.preset = args.preset
    else:
        installer.options.preset = _pick_preset(installer.options.source_dir)
    installer.options.build_dir = os.path.join(
        installer.options.source_dir, "build", installer.options.preset)
    installer.options.install_dir = os.path.join(
        installer.options.source_dir, "install", installer.options.preset)
    installer.options.generate_run_configs = True
    installer.options.generate_clion_run_configs = True
    installer.options.generate_vscode_run_configs = True
    installer.options.generate_vs_run_configs = True
    installer.options.generate_pretty_printer_configs = False
    installer.options.dry_run = False

    print(f"Refreshing IDE run configurations for preset {installer.options.preset!r}")
    if os.path.isdir(installer.options.build_dir):
        write_query(installer.options.build_dir)
    try:
        refresh(installer)
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        print(
            f"The program paths come from CMake's file API reply of "
            f"{installer.options.build_dir!r}. This run wrote the query; "
            f"configure the preset once with "
            f"`cmake --preset {installer.options.preset}` and run it again.",
            file=sys.stderr)
        return 1
    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
