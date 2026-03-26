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
from ..core.filesystem import ensure_dir, remove_dir
from ..core.process import run_cmd
from ..core.ui import TextUI, get_default_ui
from ..tools.compilers import sanitizer_flag_name
from .schema import Recipe
from .loader import recipe_placeholders, apply_placeholders, expand_path


def needs_libcxx_runtimes(sanitizer: str, compiler_id: str) -> bool:
    """
    Check if libc++ runtimes need to be built with sanitizer instrumentation.

    Only needed for clang (not apple-clang, gcc, msvc) with address or memory
    sanitizers. UBSan and TSan do not require instrumented runtimes.

    Args:
        sanitizer: Sanitizer name (e.g., "address", "asan", "memory", "msan").
        compiler_id: CMake compiler ID (e.g., "Clang", "AppleClang", "GNU").

    Returns:
        True if instrumented libc++ runtimes should be built.
    """
    if not sanitizer:
        return False
    san = sanitizer_flag_name(sanitizer.lower())
    if san not in ("address", "memory"):
        return False
    if compiler_id.lower() != "clang":
        return False
    return True


def build_libcxx_runtimes(
    recipe: Recipe,
    cc: str = "",
    cxx: str = "",
    sanitizer: str = "",
    dry_run: bool = False,
    verbose: bool = False,
    debug: bool = False,
    env: Optional[dict] = None,
    ui: Optional[TextUI] = None,
):
    """
    Build libc++/libc++abi runtimes with sanitizer instrumentation.

    This builds instrumented runtimes before the main LLVM build so that
    downstream builds use sanitizer-instrumented standard libraries.

    The runtimes are built from <llvm-source>/runtimes and installed into
    the LLVM install prefix. The runtimes build directory is cleaned up
    after installation.

    Args:
        recipe: The LLVM recipe (provides source_dir, build_dir, install_dir).
        cc: C compiler path.
        cxx: C++ compiler path.
        sanitizer: Sanitizer name (address or memory).
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

    san = sanitizer_flag_name(sanitizer.lower())

    # Map to LLVM_USE_SANITIZER values
    llvm_san_map = {
        "address": "Address",
        "memory": "MemoryWithOrigins",
    }
    llvm_sanitizer = llvm_san_map.get(san)
    if not llvm_sanitizer:
        raise ValueError(f"Unsupported sanitizer '{sanitizer}' for libc++ runtimes build")

    runtimes_src = os.path.join(recipe.source_dir, "runtimes")
    runtimes_build = recipe.build_dir + "-runtimes"
    install_prefix = recipe.install_dir

    # Determine runtimes to build
    runtimes = "libcxx;libcxxabi"
    if is_windows():
        runtimes = "libcxx"

    ui.info(f"Building libc++ runtimes with {llvm_sanitizer} sanitizer instrumentation...")

    ensure_dir(runtimes_build, dry_run=dry_run, ui=ui)

    # Configure
    cfg_cmd = [
        cmake_exe, "-S", runtimes_src, "-B", runtimes_build,
        f"-DLLVM_ENABLE_RUNTIMES={runtimes}",
        f"-DLLVM_USE_SANITIZER={llvm_sanitizer}",
        "-DLIBCXXABI_USE_LLVM_UNWINDER=OFF",
        "-DLIBCXX_INCLUDE_TESTS=OFF",
        "-DLIBCXXABI_INCLUDE_TESTS=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
    ]
    if cc:
        cfg_cmd.append(f"-DCMAKE_C_COMPILER={cc}")
    if cxx:
        cfg_cmd.append(f"-DCMAKE_CXX_COMPILER={cxx}")

    run_cmd(cfg_cmd, tail=True, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

    # Build
    build_cmd = [cmake_exe, "--build", runtimes_build]
    try:
        parallel_level = max(1, os.cpu_count() or 1)
        build_cmd.extend(["--parallel", str(parallel_level)])
    except Exception:
        pass

    run_cmd(build_cmd, tail=True, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

    # Install
    inst_cmd = [cmake_exe, "--install", runtimes_build]
    run_cmd(inst_cmd, tail=True, dry_run=dry_run, verbose=verbose, debug=debug, env=env, ui=ui)

    # Clean up runtimes build directory
    ui.info("Cleaning up runtimes build directory...")
    remove_dir(runtimes_build, dry_run=dry_run, ui=ui)

    ui.ok("libc++ runtimes built and installed successfully.")


def libcxx_runtime_flags(install_prefix: str) -> dict:
    """
    Compute downstream compiler/linker flags for using instrumented libc++ runtimes.

    After building libc++ with sanitizer instrumentation, downstream builds need
    these flags to use the instrumented standard library instead of the system one.

    Args:
        install_prefix: The LLVM install prefix where libc++ was installed.

    Returns:
        Dict with 'cxxflags' and 'ldflags' strings to merge into downstream builds.
    """
    include_dir = os.path.join(install_prefix, "include", "c++", "v1")
    lib_dir = os.path.join(install_prefix, "lib")

    cxxflags = f"-nostdinc++ -nostdlib++ -isystem {include_dir}"

    if is_windows():
        ldflags = f"-L{lib_dir} -lc++"
    else:
        ldflags = f"-L{lib_dir} -lc++abi -lc++ -Wl,-rpath,{lib_dir}"

    return {"cxxflags": cxxflags, "ldflags": ldflags}


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
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
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
        cflags: Extra C compiler flags.
        cxxflags: Extra C++ compiler flags.
        ldflags: Extra linker flags.
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
    san_c_flags = ""
    san_cxx_flags = ""
    san_ld_flags = ""
    if sanitizer:
        san = sanitizer.lower()

        # Always set the -fsanitize= compiler/linker flags
        flag_name = sanitizer_flag_name(san)
        if is_windows():
            flag = f"/fsanitize={flag_name}" if flag_name == "address" else None
        else:
            flag = f"-fsanitize={flag_name}" if flag_name else None
        if flag:
            san_c_flags = flag
            san_cxx_flags = flag
            san_ld_flags = flag

        # Add recipe-specific CMake options from the sanitizer map
        if san_map:
            reverse_alias = {
                "address": "asan", "undefined": "ubsan",
                "memory": "msan", "thread": "tsan",
            }
            extra = san_map.get(san) or san_map.get(reverse_alias.get(san, ""))
            if extra is None:
                raise ValueError(f"Unknown sanitizer '{sanitizer}' for recipe '{recipe.name}'.")
            extra_opts = apply_placeholders(extra, placeholders)
            if isinstance(extra_opts, list):
                opts.extend(extra_opts)
            else:
                opts.append(extra_opts)

    # Suppress warnings for dependency builds (not our code).
    # Prepend so user-provided flags can override (last flag wins).
    # We use is_windows() as a proxy for MSVC because MrDocs only
    # builds with MSVC on Windows (no MinGW/Clang-CL support).
    suppress_warnings = "/w" if is_windows() else "-w"

    # Merge: suppress-warnings + sanitizer flags + user-provided flags
    merged_c_flags = (suppress_warnings + " " + san_c_flags + " " + cflags).strip()
    merged_cxx_flags = (suppress_warnings + " " + san_cxx_flags + " " + cxxflags).strip()
    merged_ld_flags = (san_ld_flags + " " + ldflags).strip()

    if merged_c_flags:
        opts.append(f"-DCMAKE_C_FLAGS_INIT={merged_c_flags}")
    if merged_cxx_flags:
        opts.append(f"-DCMAKE_CXX_FLAGS_INIT={merged_cxx_flags}")
    if merged_ld_flags:
        opts.append(f"-DCMAKE_EXE_LINKER_FLAGS_INIT={merged_ld_flags}")
        opts.append(f"-DCMAKE_SHARED_LINKER_FLAGS_INIT={merged_ld_flags}")

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
    cflags: str = "",
    cxxflags: str = "",
    ldflags: str = "",
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
        cflags: Extra C compiler flags.
        cxxflags: Extra C++ compiler flags.
        ldflags: Extra linker flags.
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
                sanitizer, cflags, cxxflags, ldflags,
                force, dry_run, verbose, debug, env, ui
            )
        elif step_type == "command":
            run_command_recipe_step(
                recipe, raw_step, source_dir, third_party_src_dir,
                preset, cc, cxx, build_dir_opt, install_dir_opt,
                dry_run, verbose, debug, env, ui
            )
        else:
            raise RuntimeError(f"Unsupported build step type '{step_type}' in recipe '{recipe.name}'")
