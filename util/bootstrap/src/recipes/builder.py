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
Recipe building utilities.

Provides functions to execute build steps defined in recipe files.
"""

import os
import shutil
from typing import Optional, Dict, Any

from ..core.platform import is_windows
from ..core.filesystem import ensure_dir
from ..core.process import run_cmd
from ..core.ui import TextUI, get_default_ui
from .schema import Recipe
from .loader import recipe_placeholders, apply_placeholders, expand_path


def run_cmake_recipe_step(
    recipe: Recipe,
    step: Dict[str, Any],
    source_dir: str,
    third_party_src_dir: str,
    preset: str,
    cc: str = "",
    cxx: str = "",
    build_dir_opt: str = "",
    install_dir_opt: str = "",
    sanitizer: str = "",
    force: bool = False,
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[dict] = None,
    ui: Optional[TextUI] = None,
):
    """
    Execute a CMake build step for a recipe.

    Args:
        recipe: The recipe being built.
        step: The build step configuration.
        source_dir: MrDocs source directory.
        third_party_src_dir: Third-party sources directory.
        preset: Build preset name.
        cc: C compiler path.
        cxx: C++ compiler path.
        build_dir_opt: Project build directory.
        install_dir_opt: Project install directory.
        sanitizer: Sanitizer to use.
        force: If True, clean before building.
        dry_run: If True, only print what would be done.
        verbose: If True, show verbose output.
        debug: If True, show debug output.
        env: Environment variables for commands.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    cmake_exe = shutil.which("cmake")
    if not cmake_exe:
        raise RuntimeError("cmake executable not found in PATH.")

    placeholders = recipe_placeholders(
        recipe, preset, cc, cxx, build_dir_opt, install_dir_opt
    )

    opts = apply_placeholders(step.get("options", []), placeholders)
    build_dir = expand_path(
        step.get("build_dir", recipe.build_dir),
        source_dir, third_party_src_dir, recipe.build_type
    )
    src_dir = expand_path(
        step.get("source_dir", recipe.source_dir),
        source_dir, third_party_src_dir, recipe.build_type
    )
    source_subdir = step.get("source_subdir")
    if source_subdir:
        src_dir = os.path.join(src_dir, apply_placeholders(source_subdir, placeholders))

    generator = step.get("generator")
    config = apply_placeholders(step.get("config", recipe.build_type), placeholders)
    targets = apply_placeholders(step.get("targets", []), placeholders)
    install_flag = step.get("install", True)

    # Handle sanitizer-specific options
    san_map = step.get("sanitizers", {})
    if sanitizer:
        san = sanitizer.lower()
        if san_map:
            extra = san_map.get(san)
            if extra is None:
                raise ValueError(f"Unknown sanitizer '{sanitizer}' for recipe '{recipe.name}'.")
            extra_opts = apply_placeholders(extra, placeholders)
            if isinstance(extra_opts, list):
                opts.extend(extra_opts)
            else:
                opts.append(extra_opts)
        else:
            # Fallback: apply typical compiler sanitizer flags
            if is_windows():
                msvc_flags = {
                    "asan": "/fsanitize=address",
                }
                flag = msvc_flags.get(san)
            else:
                posix_flags = {
                    "asan": "-fsanitize=address",
                    "ubsan": "-fsanitize=undefined",
                    "msan": "-fsanitize=memory",
                    "tsan": "-fsanitize=thread",
                }
                flag = posix_flags.get(san)

            if flag:
                opts.extend([
                    f"-DCMAKE_C_FLAGS_INIT={flag}",
                    f"-DCMAKE_CXX_FLAGS_INIT={flag}",
                    f"-DCMAKE_EXE_LINKER_FLAGS_INIT={flag}",
                    f"-DCMAKE_SHARED_LINKER_FLAGS_INIT={flag}",
                ])

    ensure_dir(build_dir, dry_run=dry_run, ui=ui)

    # Configure
    cfg_cmd = [cmake_exe, "-S", src_dir, "-B", build_dir]
    if generator:
        cfg_cmd.extend(["-G", generator])
    cfg_cmd.append(f"-DCMAKE_BUILD_TYPE={config}")
    cfg_cmd.append(f"-DCMAKE_INSTALL_PREFIX={recipe.install_dir}")
    if cc:
        cfg_cmd.append(f"-DCMAKE_C_COMPILER={cc}")
    if cxx:
        cfg_cmd.append(f"-DCMAKE_CXX_COMPILER={cxx}")
    cfg_cmd.extend(opts)

    run_cmd(cfg_cmd, tail=True, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

    # Build
    build_cmd = [cmake_exe, "--build", build_dir]
    if config:
        build_cmd.extend(["--config", config])
    if targets:
        build_cmd.extend(["--target", *targets])

    # Use available cores
    if "--parallel" not in build_cmd:
        try:
            parallel_level = max(1, os.cpu_count() or 1)
            build_cmd.extend(["--parallel", str(parallel_level)])
        except Exception:
            pass

    if force:
        build_cmd.extend(["--clean-first"])

    run_cmd(build_cmd, tail=True, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

    # Install
    if install_flag:
        inst_cmd = [cmake_exe, "--install", build_dir]
        if config:
            inst_cmd.extend(["--config", config])
        run_cmd(inst_cmd, tail=True, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)


def run_command_recipe_step(
    recipe: Recipe,
    step: Dict[str, Any],
    source_dir: str,
    third_party_src_dir: str,
    preset: str,
    cc: str = "",
    cxx: str = "",
    build_dir_opt: str = "",
    install_dir_opt: str = "",
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[dict] = None,
    ui: Optional[TextUI] = None,
):
    """
    Execute a command build step for a recipe.

    Args:
        recipe: The recipe being built.
        step: The build step configuration.
        source_dir: MrDocs source directory.
        third_party_src_dir: Third-party sources directory.
        preset: Build preset name.
        cc: C compiler path.
        cxx: C++ compiler path.
        build_dir_opt: Project build directory.
        install_dir_opt: Project install directory.
        dry_run: If True, only print what would be done.
        verbose: If True, show verbose output.
        debug: If True, show debug output.
        env: Environment variables for commands.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    placeholders = recipe_placeholders(
        recipe, preset, cc, cxx, build_dir_opt, install_dir_opt
    )

    command = apply_placeholders(step.get("command", []), placeholders)
    cwd = step.get("cwd")
    if cwd:
        cwd = expand_path(
            apply_placeholders(cwd, placeholders),
            source_dir, third_party_src_dir, recipe.build_type
        )

    step_env = step.get("env")
    if step_env:
        step_env = {k: apply_placeholders(v, placeholders) for k, v in step_env.items()}
        if env:
            step_env.update(env)
        env = step_env

    run_cmd(command, cwd=cwd, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)


def build_recipe(
    recipe: Recipe,
    source_dir: str,
    third_party_src_dir: str,
    preset: str,
    cc: str = "",
    cxx: str = "",
    build_dir_opt: str = "",
    install_dir_opt: str = "",
    sanitizer: str = "",
    force: bool = False,
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[dict] = None,
    ui: Optional[TextUI] = None,
):
    """
    Build a recipe by executing all its build steps.

    Args:
        recipe: The recipe to build.
        source_dir: MrDocs source directory.
        third_party_src_dir: Third-party sources directory.
        preset: Build preset name.
        cc: C compiler path.
        cxx: C++ compiler path.
        build_dir_opt: Project build directory.
        install_dir_opt: Project install directory.
        sanitizer: Sanitizer to use.
        force: If True, clean before building.
        dry_run: If True, only print what would be done.
        verbose: If True, show verbose output.
        debug: If True, show debug output.
        env: Environment variables for commands.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    for raw_step in (recipe.build or []):
        step_type = raw_step.get("type", "").lower()
        if step_type == "cmake":
            run_cmake_recipe_step(
                recipe, raw_step, source_dir, third_party_src_dir,
                preset, cc, cxx, build_dir_opt, install_dir_opt,
                sanitizer, force, dry_run, verbose, debug, env, ui
            )
        elif step_type == "command":
            run_command_recipe_step(
                recipe, raw_step, source_dir, third_party_src_dir,
                preset, cc, cxx, build_dir_opt, install_dir_opt,
                dry_run, verbose, debug, env, ui
            )
        else:
            raise RuntimeError(f"Unsupported build step type '{step_type}' in recipe '{recipe.name}'")
