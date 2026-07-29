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
"""Longer per-option descriptions for the bootstrap CLI.

Each `add_argument(...)` call in `__main__.py` carries a short `help` string
(the brief shown in `--help` output and in the docs summary table). This
module pairs that brief with a longer paragraph used wherever the docs need
to explain the option in depth, such as the install page's option reference.
The dict is keyed by argparse `dest` so paired flags (`--build-tests` and
`--no-build-tests`) share one explanation.
"""

# Most defaults render fine straight from `InstallOptions`. A few are
# internal template strings the build engine expands at runtime (e.g.
# `<build-type:lower>-<os:lower><"-":if(cc)>...`). Those make sense in
# the Python code but look like noise in user-facing docs, so the
# generator looks here first when rendering the default cell.
OPTION_DEFAULT_DISPLAY: dict[str, str] = {
    "preset": (
        "a name like `release-macos` or `debug-linux-clang-asan`, "
        "derived from the build type, OS, compiler, and sanitizer"
    ),
    "build_dir": "`<source-dir>/build/<preset>`",
    "install_dir": "`<source-dir>/install/<preset>`",
}


OPTION_DETAILS: dict[str, str] = {
    # Build configuration
    "build_type": (
        "Selects the CMake build configuration. Release maximizes "
        "optimization; Debug disables it and enables assertions; "
        "RelWithDebInfo produces optimized binaries with debug info; "
        "MinSizeRel optimizes for size; DebugFast trades some "
        "debuggability for build speed."
    ),
    "preset": (
        "Name of the CMake preset to use. The default template "
        "expands to a value derived from build type, OS, compiler, "
        "and sanitizer; override only when you have a custom preset."
    ),
    "sanitizer": (
        "Builds Mr.Docs with a Clang/GCC sanitizer enabled. "
        "`address` catches out-of-bounds and use-after-free, "
        "`undefined` catches undefined behavior, `thread` catches "
        "data races, `memory` catches uninitialized reads. Bootstrap "
        "also rebuilds libc++ with the matching instrumentation "
        "when the sanitizer needs it."
    ),
    "build_tests": (
        "Skip building the test suite (golden, unit, lint, schema, "
        "self-doc). Tests are built by default; pass `--no-build-tests` "
        "for a leaner build that skips the Java-runtime requirement of "
        "the xml-lint step."
    ),

    # Compiler options
    "cc": (
        "Path to the C compiler. Defaults to the compiler CMake "
        "discovers on the system; set this when you need to pin a "
        "specific version or pick between Clang and GCC."
    ),
    "cxx": (
        "Path to the C++ compiler. Defaults to the compiler CMake "
        "discovers on the system; set this to pin a specific version "
        "or to use a different toolchain from CC."
    ),
    "cflags": (
        "Extra flags appended to every C compile command, both for "
        "third-party recipes and for Mr.Docs itself. Useful for "
        "linker-friendly debug info (`-gz=zstd`) or coverage flags."
    ),
    "cxxflags": (
        "Extra flags appended to every C++ compile command, both for "
        "third-party recipes and for Mr.Docs itself."
    ),
    "ldflags": (
        "Extra flags appended to every link command, both for third-"
        "party recipes and for Mr.Docs itself."
    ),

    # Tool paths
    "cmake_path": (
        "Path to the CMake executable. Bootstrap discovers CMake on "
        "`PATH` by default; set this when multiple CMake versions are "
        "installed and the wrong one is picked up."
    ),
    "ninja_path": (
        "Path to the Ninja executable. Ninja is the preferred CMake "
        "generator because it produces a `compile_commands.json` "
        "automatically; bootstrap discovers it on `PATH` by default."
    ),
    "git_path": (
        "Path to Git. Required for cloning recipes and for the "
        "compilation-database CMake integration. Bootstrap discovers "
        "Git on `PATH` by default."
    ),
    "python_path": (
        "Path to the Python interpreter recipes should use for any "
        "Python steps. Defaults to the interpreter currently running "
        "bootstrap."
    ),
    "java_path": (
        "Path to a Java runtime. Required for the xml-lint test step "
        "when `--build-tests` is on; ignored otherwise."
    ),

    # Directories
    "source_dir": (
        "Root of the cloned Mr.Docs source tree. Bootstrap auto-"
        "detects this from the working directory, so you usually do "
        "not need to set it explicitly."
    ),
    "build_dir": (
        "Where CMake places intermediate build artefacts. The default "
        "template expands per build type, OS, compiler, and sanitizer "
        "so different combinations do not clobber each other."
    ),
    "install_dir": (
        "Where the final binaries and resources are installed. The "
        "default template expands per build type, OS, compiler, and "
        "sanitizer so different combinations install side-by-side."
    ),

    # Behaviour
    "non_interactive": (
        "Accept every default prompt without asking. Use this for CI, "
        "containers, and scripted installs where there is no human at "
        "the keyboard."
    ),
    "dry_run": (
        "Print every command bootstrap would run, fully resolved with "
        "absolute paths, without executing anything. Useful when you "
        "want to inspect the build steps or reproduce them manually."
    ),
    "verbose": (
        "Show extra log lines about each step bootstrap takes."
    ),
    "debug": (
        "Show full Python tracebacks on errors. Implies verbose."
    ),
    "plain_ui": (
        "Disable colors and emoji in the output. Use for CI logs and "
        "any environment that does not render terminal escape codes."
    ),
    "install_system_deps": (
        "If a required system tool (CMake, a compiler, Git, etc.) is "
        "missing, attempt to install it via `apt-get` on Linux or "
        "`brew` on macOS before continuing."
    ),

    # Dependencies
    "clean": (
        "Wipe every cached third-party dependency build and start "
        "over. The Mr.Docs build itself is also cleared."
    ),
    "force": (
        "Rebuild a recipe even when its stamp file says it is up to "
        "date. Use this after an out-of-band change to a recipe's "
        "source tree."
    ),
    "recipe_filter": (
        "Comma-separated list of recipe names to build (for example, "
        "`llvm,jerryscript`). Recipes not in the list are skipped."
    ),
    "skip_build": (
        "Install dependencies but skip the CMake configure and build "
        "of Mr.Docs itself. Useful when you only need the dependency "
        "tree populated for CI cache restoration."
    ),
    "list_recipes": (
        "Print every dependency recipe bootstrap knows about and exit."
    ),
    "cache_dir": (
        "Use a shared directory of cached dependency installs. Each "
        "recipe installs to `<cache-dir>/<recipe-name>` instead of the "
        "per-build location. Bootstrap checks stamp files and skips "
        "recipes that are already up to date, so this directory is "
        "the natural unit of CI cache restoration."
    ),
    "cache_key": (
        "Print the cache key for one recipe and exit (for example, "
        "`--cache-key llvm`). The key includes the recipe name, the "
        "OS, and the configuration that affects its build, so it can "
        "be used as a cache restore key in CI."
    ),
    "os_key": (
        "OS or container identifier baked into cache keys (for "
        "example, `ubuntu:24.04`, `macos-15`). Required for cross-"
        "machine cache reuse so that builds from different runners "
        "do not pull each other's artefacts."
    ),
    "env_file": (
        "Path where bootstrap writes computed `<recipe>_ROOT` paths "
        "and propagation flags in `key=value` format. CI uses the "
        "file to inject environment variables into the steps that "
        "follow bootstrap."
    ),
    "refresh_all": (
        "Re-run the bootstrap configuration step for every existing "
        "IDE configuration in the source tree, regenerating run "
        "configs from current options without rebuilding."
    ),

    # Run configurations
    "generate_run_configs": (
        "Skip generating IDE run/debug configurations for CLion, "
        "VS Code, and Visual Studio. Bootstrap generates configs by "
        "default so common Mr.Docs targets are one click away; pass "
        "`--no-run-configs` when you do not want bootstrap to touch "
        "the IDE config directories."
    ),
}
