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
Process execution utilities for the bootstrap tool.

Provides functions for running shell commands with proper output handling
and error reporting.
"""

import math
import os
import shlex
import shutil
import subprocess
import sys
from typing import Optional, List, Dict, Union

from .ui import TextUI, get_default_ui


def run_cmd(
    cmd: Union[str, List[str]],
    cwd: Optional[str] = None,
    tail: bool = False,
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[Dict[str, str]] = None,
    ui: Optional[TextUI] = None,
):
    """
    Run a shell command in the specified directory.

    When tail=True, only the last line of live output is shown (npm-style),
    while the full output is buffered and displayed only on failure.

    Args:
        cmd: Command to run (string or list of arguments).
        cwd: Working directory for the command.
        tail: If True, show only the last line of output during execution.
        dry_run: If True, only print what would be done without executing.
        verbose: If True, show full output on failure.
        debug: If True, raise original exception on failure.
        env: Environment variables to use (defaults to current environment).
        ui: TextUI instance for output. Uses default if not provided.

    Raises:
        RuntimeError: If the command fails.
    """
    if ui is None:
        ui = get_default_ui()
    if cwd is None:
        cwd = os.getcwd()

    display_cwd = ui.shorten_path(cwd) if cwd else os.getcwd()
    if isinstance(cmd, list):
        cmd_str = ' '.join(shlex.quote(arg) for arg in cmd)
    else:
        cmd_str = cmd

    # Always show the command with cwd for transparency
    ui.command(f"{display_cwd}> {cmd_str}", icon="\U0001f4bb")

    if dry_run:
        ui.info("dry-run: command not executed")
        return

    # Favor parallel builds unless user already set it
    effective_env = (env or os.environ).copy()
    if "CMAKE_BUILD_PARALLEL_LEVEL" not in effective_env:
        try:
            effective_env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(max(1, os.cpu_count() or 1))
        except Exception:
            effective_env["CMAKE_BUILD_PARALLEL_LEVEL"] = "4"

    if not tail:
        try:
            r = subprocess.run(cmd, shell=isinstance(cmd, str), check=True, cwd=cwd, env=effective_env)
        except subprocess.CalledProcessError as exc:
            if debug:
                raise
            tips = [
                f"Working dir: {ui.shorten_path(cwd)}",
            ]
            if not verbose:
                tips.append("Re-run with --verbose for full output")
            ui.error_block(f"Command failed: {exc}", tips)
            raise RuntimeError(f"Command '{cmd}' failed. Re-run with --debug for traceback.") from None
        if r.returncode != 0:
            raise RuntimeError(f"Command '{cmd}' failed with return code {r.returncode}.")
        return

    # tail == True: stream output but only show the last line live
    output_lines: List[str] = []
    try:
        proc = subprocess.Popen(
            cmd,
            shell=isinstance(cmd, str),
            cwd=cwd,
            env=effective_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True,
        )
    except Exception as exc:  # noqa: BLE001
        raise RuntimeError(f"Failed to launch command '{cmd}': {exc}") from None

    try:
        term_width = shutil.get_terminal_size(fallback=(80, 24)).columns or 80
        prev_height = 0
        if proc.stdout:
            for line in proc.stdout:
                line = line.rstrip("\r\n")
                output_lines.append(line + "\n")
                # compute how many terminal rows this line will wrap to
                visible = line
                height = max(1, math.ceil(len(visible) / term_width))
                # move cursor up to start of previous render and clear those rows
                if prev_height:
                    sys.stdout.write(f"\x1b[{prev_height}F")
                    for _ in range(prev_height):
                        sys.stdout.write("\x1b[2K\x1b[1E")
                    sys.stdout.write(f"\x1b[{prev_height}F")
                # render current line (letting terminal wrap naturally)
                sys.stdout.write("\x1b[2K" + line + "\n")
                sys.stdout.flush()
                prev_height = height
        proc.wait()
    finally:
        if proc.stdout:
            proc.stdout.close()

    if output_lines:
        # Ensure cursor ends on a clean line after the last render
        sys.stdout.write("\x1b[2K")
        sys.stdout.flush()

    if proc.returncode != 0:
        # On failure, show the full buffered output
        if not verbose:
            ui.error_block(
                f"Command failed: {cmd}",
                ["Working dir: " + ui.shorten_path(cwd or os.getcwd())],
            )
        print("".join(output_lines), end="")
        raise RuntimeError(f"Command '{cmd}' failed with return code {proc.returncode}.")
