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
Main installer orchestration for the MrDocs bootstrap process.

This module provides the MrDocsInstaller class that coordinates
all aspects of setting up the MrDocs development environment.
"""

import dataclasses
import os
import re
import shlex
from typing import Optional, Dict, Any, List, Set


def _shquote(s: str) -> str:
    """Shell-quote a string, returning it unquoted if safe."""
    return shlex.quote(s)

from .core import (
    TextUI,
    InstallOptions,
    get_source_dir,
    get_os_name,
    is_windows,
    set_default_ui,
    prompt_string,
    prompt_boolean,
    prompt_choice,
    run_cmd,
    check_git_symlinks,
    BUILD_TYPES,
    SANITIZERS,
)
from .tools import find_tool, probe_compilers, install_ninja, is_tool_executable, probe_msvc_dev_env
from .tools.compilers import sanitizer_flag_name
from .tools.prerequisites import check_prerequisites, report_missing_prerequisites, try_install_system_deps
from .recipes import (
    Recipe,
    load_recipe_files,
    topo_sort_recipes,
    fetch_recipe_source,
    apply_recipe_patches,
    build_recipe,
    build_libcxx_runtimes,
    needs_libcxx_runtimes,
    libcxx_runtime_flags,
    write_recipe_stamp,
    recipe_stamp_path,
    is_recipe_up_to_date,
    generate_cache_key,
    detect_compiler_for_cache_key,
)
from .presets import create_cmake_presets
from .configs import generate_run_configs, generate_pretty_printer_configs


class MrDocsInstaller:
    """
    Handles the installation workflow for MrDocs and its third-party dependencies.

    This class coordinates:
    - User prompts for configuration options
    - Tool detection (compilers, CMake, Ninja, etc.)
    - Dependency fetching and building via recipes
    - CMake preset generation
    - IDE run configuration generation
    """

    def __init__(self, cmd_line_args: Optional[Dict[str, Any]] = None, source_dir: Optional[str] = None):
        """
        Initialize the installer with command-line arguments.

        Args:
            cmd_line_args: Dictionary of command-line arguments.
            source_dir: Override source directory (useful for tests).
        """
        self.cmd_line_args = cmd_line_args or {}
        self.default_options = InstallOptions()
        self.options = InstallOptions()

        # Initialize string fields to empty, booleans keep their defaults
        for field in dataclasses.fields(self.options):
            if field.type == str:
                setattr(self.options, field.name, '')

        # Seed critical defaults
        self.options.source_dir = source_dir or get_source_dir()
        self.options.third_party_src_dir = os.path.join(self.options.source_dir, "build", "third-party")

        # Paths
        self.recipes_dir = os.path.join(self.options.source_dir, "third-party", "recipes")
        self.patches_dir = os.path.join(self.options.source_dir, "third-party", "patches")

        # Apply command-line arguments
        for field in dataclasses.fields(self.options):
            name = field.name
            if name in self.cmd_line_args and self.cmd_line_args[name] is not None:
                setattr(self.options, name, self.cmd_line_args[name])

        self.options.non_interactive = self.cmd_line_args.get("non_interactive", False)

        # State tracking
        self.prompted_options: Set[str] = set()
        self.compiler_info: Dict[str, str] = {}
        self.package_roots: Dict[str, str] = {}
        self.recipe_info: Dict[str, Recipe] = {}
        self.rebuilt_recipes: List[str] = []
        self._libcxx_cxxflags: str = ""
        self._libcxx_ldflags: str = ""
        self.env = os.environ.copy()
        self.env["PKG_CONFIG"] = "false"  # Avoid CMake regex issues

        # Dry-run implies non-interactive (prompts would block script generation)
        if self.options.dry_run:
            self.options.non_interactive = True

        # UI setup
        plain_ui = bool(self.cmd_line_args.get("plain_ui", False))
        self.ui = TextUI(enable_color=not plain_ui, enable_emoji=not plain_ui)
        self.ui.set_base_path(self.options.source_dir)
        self.ui.dry_run = self.options.dry_run
        set_default_ui(self.ui)

    def prompt_option(self, name: str, prompt_text: str, force_prompt: bool = False):
        """
        Prompt the user for a configuration option.

        Args:
            name: Option name (field in InstallOptions).
            prompt_text: Prompt text to display.
            force_prompt: If True, prompt even if already prompted.

        Returns:
            The option value.
        """
        name = name.replace("-", "_")

        if name in self.prompted_options and not force_prompt:
            return getattr(self.options, name)

        default_value = getattr(self.default_options, name, None)
        if default_value is None:
            raise ValueError(f"Option '{name}' not found in default options.")

        # Check command-line args
        if name in self.cmd_line_args:
            value = self.cmd_line_args[name]
            setattr(self.options, name, value)
            self.prompted_options.add(name)
            return value

        # Expand placeholders in default value
        if isinstance(default_value, str):
            default_value = self._expand_placeholders(default_value)
            setattr(self.default_options, name, default_value)

        # Non-interactive mode
        if self.options.non_interactive:
            display = self.ui.maybe_shorten(default_value) if isinstance(default_value, str) else default_value
            self.ui.info(f"{prompt_text}: {display}")
            setattr(self.options, name, default_value)
            self.prompted_options.add(name)
            return default_value

        # Interactive prompt
        if isinstance(getattr(self.default_options, name), bool):
            value = prompt_boolean(prompt_text, default_value, non_interactive=False)
        else:
            value = prompt_string(prompt_text, default_value, non_interactive=False, ui=self.ui)

        setattr(self.options, name, value)
        self.prompted_options.add(name)
        return value

    def reprompt_option(self, name: str, prompt_text: str):
        """Force re-prompt for an option."""
        return self.prompt_option(name, prompt_text, force_prompt=True)

    def prompt_validated_option(
        self,
        name: str,
        prompt_text: str,
        valid_values: list,
        normalizer=None,
        allow_empty: bool = False,
    ):
        """
        Prompt for an option with validation and retry support.

        Args:
            name: The option name.
            prompt_text: The prompt to display.
            valid_values: List of valid values (canonical forms).
            normalizer: Optional function to normalize input before comparison.
            allow_empty: If True, empty input is accepted (returns '').

        Returns:
            The validated value.
        """
        if normalizer is None:
            normalizer = lambda v: v.lower()

        def match_value(input_val):
            if allow_empty and (not input_val or input_val.lower() == "none"):
                return ''
            for v in valid_values:
                if normalizer(v) == normalizer(input_val):
                    return v
            return None

        valid_display = ', '.join(valid_values)
        if allow_empty:
            valid_display += ', or none'

        for attempt in range(3):  # Allow 3 attempts
            if attempt == 0:
                value = self.prompt_option(name, prompt_text)
            else:
                value = self.reprompt_option(name, prompt_text)

            matched = match_value(value)
            if matched is not None:
                setattr(self.options, name, matched)
                return matched

            self.ui.warn(f"Invalid {name.replace('_', ' ')} '{value}'. Must be one of: {valid_display}.")

        raise ValueError(f"Invalid {name.replace('_', ' ')} '{value}'. Must be one of: {valid_display}.")

    def prompt_build_type_option(self, name: str):
        """Prompt for build type with validation."""
        # Note: OptimizedDebug is internal-only (used for MSVC + DebugFast)
        valid_build_types = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel", "DebugFast"]
        normalizer = lambda v: v.lower().replace("-", "")
        return self.prompt_validated_option(name, "Build type", valid_build_types, normalizer=normalizer)

    def prompt_sanitizer_option(self, name: str):
        """Prompt for sanitizer with validation."""
        valid_sanitizers = ["ASan", "UBSan", "MSan", "TSan", "address", "undefined", "memory", "thread"]
        return self.prompt_validated_option(name, "Sanitizer", valid_sanitizers, allow_empty=True)

    def _expand_placeholders(self, template: str) -> str:
        """Expand placeholders in a template string."""
        if "<" not in template or ">" not in template:
            return template

        def repl(match):
            key = match.group(1)
            transform_fn = match.group(2)

            # Literal strings
            if key.startswith('"') and key.endswith('"'):
                val = key[1:-1]
            elif key == 'os':
                val = get_os_name()
            else:
                key = key.replace("-", "_")
                val = getattr(self.options, key, "")

            if transform_fn:
                if transform_fn == "lower":
                    val = val.lower() if val else ""
                elif transform_fn == "upper":
                    val = val.upper() if val else ""
                elif transform_fn == "basename":
                    val = os.path.splitext(os.path.basename(val))[0] if val else ""
                elif transform_fn.startswith("if(") and transform_fn.endswith(")"):
                    var_name = transform_fn[3:-1]
                    if getattr(self.options, var_name, None):
                        val = val.lower() if val else ""
                    else:
                        val = ""

            return val

        pattern = r"<([\"a-zA-Z0-9_\-]+)(?::([a-zA-Z0-9_\-\(\)]+))?>"
        result = re.sub(pattern, repl, template)

        # Make paths absolute if they contain dir references
        if "-dir" in template.lower() and result and not os.path.isabs(result):
            result = os.path.abspath(result)

        return result

    def check_tool(self, tool: str):
        """Check that a required tool is available with re-prompting on invalid input."""
        default_value = find_tool(tool) or tool
        setattr(self.default_options, f"{tool}_path", default_value)

        # In dry-run mode, accept the default/CLI path without validation so
        # the script can be generated even when tools aren't installed locally.
        if self.options.dry_run:
            tool_path = self.prompt_option(f"{tool}_path", tool)
            return tool_path

        for attempt in range(3):
            if attempt == 0:
                tool_path = self.prompt_option(f"{tool}_path", tool)
            else:
                tool_path = self.reprompt_option(f"{tool}_path", tool)

            if is_tool_executable(tool_path):
                return tool_path

            self.ui.warn(f"'{tool_path}' is not a valid {tool} executable.")

        raise FileNotFoundError(f"{tool} executable not found after multiple attempts.")

    def check_system_prerequisites(self):
        """Check system prerequisites and report/install missing ones."""
        # In dry-run mode, skip actual checks so the script can be generated
        # even when prerequisites aren't installed locally.  The generated
        # script lists what the user needs in a comment.
        if self.options.dry_run:
            prereqs = ["cmake", "git", "python3", "a C/C++ compiler"]
            if self.options.build_tests:
                prereqs.append("java")
            print(f"# Verify prerequisites: {', '.join(prereqs)}")
            return

        missing = check_prerequisites(
            build_tests=self.options.build_tests,
            cc=self.options.cc,
            cxx=self.options.cxx,
            ui=self.ui,
        )

        if not missing:
            self.ui.ok("All system prerequisites found.")
            return

        if self.options.install_system_deps:
            still_missing = try_install_system_deps(
                missing,
                non_interactive=self.options.non_interactive,
                ui=self.ui,
            )
            if still_missing:
                report_missing_prerequisites(still_missing, self.ui)
                required_missing = [p for p in still_missing if p.required]
                if required_missing:
                    raise RuntimeError(
                        "Required system tools are missing. "
                        "Install them and re-run bootstrap."
                    )
        else:
            report_missing_prerequisites(missing, self.ui)
            required_missing = [p for p in missing if p.required]
            if required_missing:
                raise RuntimeError(
                    "Required system tools are missing. "
                    "Install them and re-run bootstrap, "
                    "or use --install-system-deps to install automatically."
                )

    def check_tools(self):
        """Check all required tools."""
        for tool in ["git", "cmake", "python"]:
            self.check_tool(tool)

    def prompt_compiler_option(self, name: str, prompt_text: str):
        """Prompt for a compiler path with validation and re-prompting."""
        # In dry-run mode, accept the path without validation so the script
        # can be generated even when the compiler isn't installed locally.
        if self.options.dry_run:
            return self.prompt_option(name, prompt_text)

        for attempt in range(3):
            if attempt == 0:
                compiler_path = self.prompt_option(name, prompt_text)
            else:
                compiler_path = self.reprompt_option(name, prompt_text)

            # Empty is allowed (will use system default)
            if not compiler_path:
                return compiler_path

            if is_tool_executable(compiler_path):
                return compiler_path

            self.ui.warn(f"'{compiler_path}' is not a valid compiler executable.")

        raise FileNotFoundError(f"Valid {prompt_text.lower()} not found after multiple attempts.")

    def setup_compilers(self):
        """Set up and validate compilers."""
        self.prompt_compiler_option("cc", "C compiler")
        self.prompt_compiler_option("cxx", "C++ compiler")

    def setup_build_options(self):
        """Set up build-related options."""
        self.prompt_build_type_option("build_type")
        self.prompt_sanitizer_option("sanitizer")
        if self.prompt_option("build_tests", "Build tests"):
            self.check_tool("java")

    def install_dependencies(self):
        """Install third-party dependencies using recipes."""
        recipes = load_recipe_files(
            self.recipes_dir,
            self.options.source_dir,
            self.options.preset,
            self.options.build_type,
            self.options.cc,
            self.options.cxx,
            self.options.build_dir,
            self.options.install_dir,
            self.ui,
        )

        if not recipes:
            raise RuntimeError(f"No recipes found in {self.recipes_dir}")

        # Override install dirs when --cache-dir is specified
        if self.options.cache_dir:
            cache_dir = os.path.abspath(self.options.cache_dir)
            for recipe in recipes:
                recipe.install_dir = os.path.join(cache_dir, recipe.name)

        # Collect valid package root variable names from all recipes
        # (before filtering) so stale roots can be cleaned from presets
        self.valid_package_root_vars = [
            r.package_root_var for r in recipes if r.package_root_var
        ]

        if self.options.recipe_filter:
            wanted = {n.strip().lower() for n in self.options.recipe_filter.split(",") if n.strip()}
            recipes = [r for r in recipes if r.name.lower() in wanted]

        ordered = topo_sort_recipes(recipes)

        for recipe in ordered:
            self.ui.section(f"Installing {recipe.name}")
            self._dry_comment(f"=== Install dependency: {recipe.name} {recipe.version} ===")
            self._dry_comment(f"  source: {recipe.source_dir}")
            self._dry_comment(f"  install: {recipe.install_dir}")

            # Check resolved ref for up-to-date check
            src = recipe.source
            resolved_ref = src.commit or src.tag or src.branch or src.ref or ""

            # Only pass sanitizer to dependency builds when the sanitizer
            # actually requires instrumented dependencies.  ASan, MSan, and
            # TSan need instrumented deps (ASan/MSan for libc++, TSan for
            # correct race detection across library boundaries).  UBSan
            # only checks the project's own code at compile time, so passing
            # it to deps would cause unnecessary rebuilds.
            compiler_id = self.compiler_info.get("CMAKE_CXX_COMPILER_ID", "")
            san = sanitizer_flag_name(self.options.sanitizer.lower()) if self.options.sanitizer else ""
            deps_need_sanitizer = (
                compiler_id.lower() == "clang"
                and san in ("address", "memory", "thread")
            )
            recipe_sanitizer = self.options.sanitizer if deps_need_sanitizer else ""

            # Parameters that affect the build output, used for stamp
            # hashing.  Only include build parameters when the sanitizer
            # actually requires instrumented deps (ASan/MSan/TSan + Clang).
            # For all other builds, the only flag reaching deps is -gz=zstd
            # (debug info compression), which doesn't change library
            # behavior, so the stamp just checks version and ref.
            if deps_need_sanitizer:
                stamp_args = dict(
                    sanitizer=recipe_sanitizer,
                    cc=self.options.cc, cxx=self.options.cxx,
                    cflags=self.options.cflags, cxxflags=self.options.cxxflags,
                    ldflags=self.options.ldflags,
                )
            else:
                stamp_args = dict()

            # For LLVM with sanitizers: always compute libc++ flags
            # even on cache hit, since downstream builds need them.
            if recipe.name == "llvm":
                if needs_libcxx_runtimes(self.options.sanitizer, compiler_id):
                    rt_flags = libcxx_runtime_flags(recipe.install_dir)
                    self._libcxx_cxxflags = rt_flags["cxxflags"]
                    self._libcxx_ldflags = rt_flags["ldflags"]
                    self.options.cxxflags = (self.options.cxxflags + " " + rt_flags["cxxflags"]).strip()
                    self.options.ldflags = (self.options.ldflags + " " + rt_flags["ldflags"]).strip()

            # Skip build if already up to date (unless force, clean, or dry-run)
            # In dry-run mode, always show all commands so the output is a
            # complete manual reference regardless of local cache state.
            stale_reason = is_recipe_up_to_date(recipe, resolved_ref, **stamp_args) if not self.options.dry_run else "dry-run"
            if not stale_reason and not self.options.force and not self.options.clean:
                self.ui.ok(f"[{recipe.name}] already up to date ({resolved_ref or 'HEAD'}). Skipping build.")
                self.print_recipe_summary(recipe)
                self.recipe_info[recipe.name] = recipe
                if recipe.package_root_var:
                    self.package_roots[recipe.package_root_var] = recipe.install_dir
                continue

            if stale_reason:
                self.ui.info(f"[{recipe.name}] rebuilding: {stale_reason}")
                stamp_path = recipe_stamp_path(recipe)
                if os.path.exists(stamp_path):
                    with open(stamp_path, "r", encoding="utf-8") as f:
                        self.ui.info(f"[{recipe.name}] existing stamp ({stamp_path}):\n{f.read()}")

            self._dry_comment(f"Fetch {recipe.name} source")
            fetch_recipe_source(
                recipe,
                self.options.source_dir,
                self.options.git_path or "git",
                clean=self.options.clean,
                force=self.options.force,
                dry_run=self.options.dry_run,
                verbose=self.options.verbose,
                debug=self.options.debug,
                env=self.env,
                ui=self.ui,
            )

            self._dry_comment(f"Apply patches for {recipe.name}")
            apply_recipe_patches(
                recipe,
                self.patches_dir,
                dry_run=self.options.dry_run,
                verbose=self.options.verbose,
                debug=self.options.debug,
                env=self.env,
                ui=self.ui,
            )

            # Build libc++ runtimes if needed (LLVM + clang + asan/msan/tsan)
            extra_cmake_options = None
            if recipe.name == "llvm":
                compiler_id = self.compiler_info.get("CMAKE_CXX_COMPILER_ID", "")
                if needs_libcxx_runtimes(self.options.sanitizer, compiler_id):
                    self.ui.subsection("Building libc++ runtimes")
                    build_libcxx_runtimes(
                        recipe,
                        self.options.cc,
                        self.options.cxx,
                        self.options.sanitizer,
                        self.options.dry_run,
                        self.options.verbose,
                        self.options.debug,
                        self.env,
                        self.ui,
                    )
                    # Disable runtimes in the main LLVM build so it doesn't
                    # overwrite the instrumented ones we just built
                    extra_cmake_options = ["-DLLVM_ENABLE_RUNTIMES="]

            self._dry_comment(f"Build and install {recipe.name}")
            build_recipe(
                recipe,
                self.options.source_dir,
                self.options.third_party_src_dir,
                self.options.preset,
                self.options.cc,
                self.options.cxx,
                self.options.build_dir,
                self.options.install_dir,
                recipe_sanitizer,
                self.options.cflags,
                self.options.cxxflags,
                self.options.ldflags,
                extra_cmake_options,
                self.options.force,
                self.options.dry_run,
                self.options.verbose,
                self.options.debug,
                self.env,
                self.ui,
            )

            write_recipe_stamp(recipe, resolved_ref, **stamp_args, dry_run=self.options.dry_run, ui=self.ui)
            self.rebuilt_recipes.append(recipe.name)

            self.ui.ok(f"[{recipe.name}] installed successfully.")
            self.print_recipe_summary(recipe)

            self.recipe_info[recipe.name] = recipe
            if recipe.package_root_var:
                self.package_roots[recipe.package_root_var] = recipe.install_dir

    def setup_ninja(self):
        """Set up Ninja build system."""
        ninja_path = install_ninja(
            self.options.source_dir,
            self.options.preset,
            self.options.ninja_path,
            self.options.dry_run,
            self.ui,
        )
        if ninja_path:
            self.options.ninja_path = ninja_path

    def create_presets(self):
        """Create CMake user presets."""
        create_cmake_presets(
            self.options.source_dir,
            self.options.preset,
            self.options.build_type,
            self.options.cc,
            self.options.cxx,
            self.options.ninja_path,
            self.options.python_path,
            self.options.git_path,
            self.options.sanitizer,
            self.package_roots,
            self.compiler_info,
            valid_package_root_vars=getattr(self, 'valid_package_root_vars', None),
            cflags=self.options.cflags,
            cxxflags=self.options.cxxflags,
            ldflags=self.options.ldflags,
            dry_run=self.options.dry_run,
            ui=self.ui,
        )

    def generate_configs(self):
        """Generate IDE run configurations."""
        if not self.options.generate_run_configs:
            return

        generate_run_configs(
            options=self.options,
            default_options=self.default_options,
            package_roots=self.package_roots,
            compiler_info=self.compiler_info,
            generate_clion=self.options.generate_clion_run_configs,
            generate_vscode=self.options.generate_vscode_run_configs,
            generate_vs=self.options.generate_vs_run_configs,
            generate_justfile=self.options.generate_justfile_run_configs,
            dry_run=self.options.dry_run,
            ui=self.ui,
        )

        if self.options.generate_pretty_printer_configs:
            generate_pretty_printer_configs(
                source_dir=self.options.source_dir,
                dry_run=self.options.dry_run,
                ui=self.ui,
            )

    def list_recipes(self):
        """List available recipes."""
        recipes = load_recipe_files(
            self.recipes_dir,
            self.options.source_dir,
            "default",
            "Release",
            ui=self.ui,
        )
        if not recipes:
            print("No recipes found.")
            return

        print("Available recipes:")
        for recipe in recipes:
            print(f"  - {recipe.name} ({recipe.version})")
            if recipe.dependencies:
                print(f"    Dependencies: {', '.join(recipe.dependencies)}")

    def get_cache_key(self, recipe_name: str) -> str:
        """
        Generate a CI-compatible cache key for a recipe.

        Loads the recipe, determines compiler info, and returns the key.

        Args:
            recipe_name: Name of the recipe (e.g. "llvm").

        Returns:
            Cache key string.
        """
        recipes = load_recipe_files(
            self.recipes_dir,
            self.options.source_dir,
            self.options.preset or "default",
            self.options.build_type or "Release",
            ui=self.ui,
        )

        recipe = None
        for r in recipes:
            if r.name.lower() == recipe_name.lower():
                recipe = r
                break

        if not recipe:
            available = [r.name for r in recipes]
            raise RuntimeError(
                f"Recipe '{recipe_name}' not found. Available: {', '.join(available)}"
            )

        recipe_hash = recipe.source.commit or recipe.version or ""

        compiler, compiler_version = detect_compiler_for_cache_key(self.options.cc)

        os_key = self.options.os_key
        if not os_key:
            raise RuntimeError(
                "--os-key is required for cache key generation "
                "(e.g. --os-key ubuntu:24.04)"
            )

        return generate_cache_key(
            recipe_name=recipe.name,
            recipe_hash=recipe_hash,
            build_type=self.options.build_type or "Release",
            os_key=os_key,
            compiler=compiler,
            compiler_version=compiler_version,
            sanitizer=self.options.sanitizer,
        )

    def write_env_file(self):
        """
        Write computed _ROOT paths and flags to a file in key=value format.

        This is intended for CI integration: the CI can source this file
        into GITHUB_ENV so downstream steps (e.g. cmake-workflow) get the
        correct dependency paths and compiler flags without manual computation.

        Only bootstrap-computed flags are written (libc++ paths, sanitizer
        linker flag), NOT user-passed --cflags/--cxxflags/--ldflags which
        the CI already knows.  This avoids flag duplication when the CI
        combines its own matrix flags with bootstrap output.
        """
        env_path = self.options.env_file
        if not env_path:
            return

        from .tools.compilers import sanitizer_flag_name

        lines = []

        # Write _ROOT paths for each installed recipe
        for var_name, path in sorted(self.package_roots.items()):
            lines.append(f"{var_name}={path}")

        # Write bootstrap-computed CXX flags (libc++ include paths only)
        if self._libcxx_cxxflags:
            lines.append(f"BOOTSTRAP_CXXFLAGS={self._libcxx_cxxflags}")

        # Write bootstrap-computed LD flags (libc++ link paths + sanitizer)
        bootstrap_ldflags = self._libcxx_ldflags
        if self.options.sanitizer:
            flag_name = sanitizer_flag_name(self.options.sanitizer)
            if flag_name:
                san_flag = f"-fsanitize={flag_name}"
                bootstrap_ldflags = (bootstrap_ldflags + " " + san_flag).strip()
        if bootstrap_ldflags:
            lines.append(f"BOOTSTRAP_LDFLAGS={bootstrap_ldflags}")

        # Write which recipes were rebuilt (empty if all were cached)
        if self.rebuilt_recipes:
            lines.append(f"BOOTSTRAP_REBUILT={','.join(self.rebuilt_recipes)}")

        content = "\n".join(lines) + "\n" if lines else ""

        if self.options.dry_run:
            self._dry_comment("Write bootstrap env file for CI")
            print(f"cat > {_shquote(env_path)} <<'BOOTSTRAP_ENV_EOF'")
            print(content.rstrip())
            print("BOOTSTRAP_ENV_EOF")
        else:
            os.makedirs(os.path.dirname(os.path.abspath(env_path)), exist_ok=True) if os.path.dirname(env_path) else None
            with open(env_path, 'w') as f:
                f.write(content)
            self.ui.ok(f"Environment file written to {env_path}")

    def print_recipe_summary(self, recipe: Recipe):
        """Print a summary of where a dependency was installed."""
        items = [
            ("Source", self.ui.maybe_shorten(recipe.source_dir)),
            ("Build", self.ui.maybe_shorten(recipe.build_dir)),
            ("Install", self.ui.maybe_shorten(recipe.install_dir)),
        ]
        max_key_len = max(len(k) for k, _ in items)
        for key, path in items:
            self.ui.info(f"    {key.rjust(max_key_len)}: {path}")

    def print_mrdocs_summary(self):
        """Print a summary of MrDocs build configuration."""
        # Expand build_dir and install_dir with current options
        build_dir = self._expand_placeholders(self.default_options.build_dir)
        install_dir = self._expand_placeholders(self.default_options.install_dir)

        items = [
            ("Source", self.ui.maybe_shorten(self.options.source_dir)),
            ("Build", self.ui.maybe_shorten(build_dir)),
            ("Install", self.ui.maybe_shorten(install_dir)),
            ("Preset", self.options.preset),
        ]
        max_key_len = max(len(k) for k, _ in items)
        for key, value in items:
            self.ui.info(f"  {key.rjust(max_key_len)}: {value}")

    def show_preset_summary(self):
        """Display key details of the selected CMake user preset."""
        import json as json_module
        path = os.path.join(self.options.source_dir, "CMakeUserPresets.json")
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json_module.load(f)
        except Exception as exc:
            self.ui.warn(f"Could not read {self.ui.shorten_path(path)}: {exc}")
            return

        preset = None
        for p in data.get("configurePresets", []):
            if p.get("name") == self.options.preset:
                preset = p
                break

        if not preset:
            self.ui.warn(f"Preset '{self.options.preset}' not found in {self.ui.shorten_path(path)}")
            return

        cache = preset.get("cacheVariables", {})
        roots = {k: v for k, v in cache.items() if k.endswith("_ROOT")}
        summary = [
            ("Preset file", self.ui.shorten_path(path)),
            ("Preset name", preset.get("name", "")),
            ("Generator", preset.get("generator", "")),
            ("Binary dir", preset.get("binaryDir", "")),
        ]
        if roots:
            for k, v in sorted(roots.items()):
                summary.append((k, v))
        if "CMAKE_MAKE_PROGRAM" in cache:
            summary.append(("CMAKE_MAKE_PROGRAM", cache["CMAKE_MAKE_PROGRAM"]))
        self.ui.kv_block(None, summary, indent=4)

    def is_non_empty_dir(self, path: str) -> bool:
        """Check if path is a non-empty directory."""
        return os.path.exists(path) and os.path.isdir(path) and len(os.listdir(path)) > 0

    def is_abi_compatible(self, build_type_a: str, build_type_b: str) -> bool:
        """Check if two build types are ABI compatible."""
        debug_types = {"debug", "debugfast", "debug-fast"}
        release_types = {"release", "relwithdebinfo", "minsizerel", "optimizeddebug"}
        a_lower = build_type_a.lower()
        b_lower = build_type_b.lower()
        if a_lower in debug_types and b_lower in debug_types:
            return True
        if a_lower in release_types and b_lower in release_types:
            return True
        if a_lower == b_lower:
            return True
        return False

    def cmake_workflow(
        self,
        src_dir: str,
        build_type: str,
        build_dir: str,
        install_dir: str,
        extra_args: list = None,
        cc_flags: str = "",
        cxx_flags: str = "",
        ld_flags: str = "",
        force_rebuild: bool = False,
        remove_build_dir: bool = True,
        allow_skip: bool = True,
    ):
        """
        Configure, build, and install a CMake project.

        Args:
            src_dir: Source directory.
            build_type: CMake build type.
            build_dir: Build directory.
            install_dir: Install directory.
            extra_args: Extra CMake arguments.
            cc_flags: Extra C compiler flags.
            cxx_flags: Extra C++ compiler flags.
            ld_flags: Extra linker flags.
            force_rebuild: Force rebuild even if install_dir exists.
            remove_build_dir: Remove build_dir after install.
            allow_skip: Allow skipping if install_dir exists.
        """
        from .core.filesystem import remove_dir, ensure_dir

        extra_args = extra_args or []

        # Check if we can skip the build
        # In dry-run mode, always show all commands for a complete manual reference.
        if not self.options.dry_run and allow_skip and self.is_non_empty_dir(install_dir):
            if force_rebuild or self.options.force:
                self.ui.info(f"Force rebuild requested. Removing {self.ui.maybe_shorten(install_dir)}.")
                remove_dir(install_dir, dry_run=self.options.dry_run, ui=self.ui)
                if remove_build_dir and self.is_non_empty_dir(build_dir):
                    remove_dir(build_dir, dry_run=self.options.dry_run, ui=self.ui)
            else:
                self.ui.info(f"Install directory already exists. Skipping build.")
                return

        if remove_build_dir and force_rebuild and self.is_non_empty_dir(build_dir):
            remove_dir(build_dir, dry_run=self.options.dry_run, ui=self.ui)
        if self.is_non_empty_dir(install_dir):
            remove_dir(install_dir, dry_run=self.options.dry_run, ui=self.ui)

        # Extract and merge compiler flags from extra_args
        extra_args_filtered = []
        for i, arg in enumerate(extra_args):
            if arg.startswith('-DCMAKE_C_FLAGS='):
                cc_flags += ' ' + arg.split('=', 1)[1]
            elif arg.startswith('-DCMAKE_CXX_FLAGS='):
                cxx_flags += ' ' + arg.split('=', 1)[1]
            else:
                extra_args_filtered.append(arg)
        extra_args = extra_args_filtered

        cmake_exe = self.options.cmake_path or "cmake"
        config_args = [cmake_exe, "-S", src_dir]

        if build_dir:
            config_args.extend(["-B", build_dir])
            ensure_dir(build_dir, dry_run=self.options.dry_run, ui=self.ui)

        if self.options.ninja_path:
            config_args.extend(["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={self.options.ninja_path}"])

        if self.options.cc and self.options.cxx:
            config_args.extend([
                f"-DCMAKE_C_COMPILER={self.options.cc}",
                f"-DCMAKE_CXX_COMPILER={self.options.cxx}"
            ])

        # Windows-specific tool paths
        if is_windows():
            if self.options.python_path:
                config_args.append(f"-DPYTHON_EXECUTABLE={self.options.python_path}")
            if self.options.git_path:
                config_args.append(f"-DGIT_EXECUTABLE={self.options.git_path}")

        # Handle ABI compatibility
        if not self.is_abi_compatible(self.options.build_type, build_type):
            self.ui.warn(f"Build type '{build_type}' is not ABI compatible with MrDocs build type '{self.options.build_type}'.")
            if self.options.build_type.lower() in ("debug", "debugfast", "debug-fast"):
                self.ui.info("Changing to 'OptimizedDebug' for ABI compatibility.")
                build_type = "OptimizedDebug"
            else:
                self.ui.info(f"Changing to '{self.options.build_type}' for ABI compatibility.")
                build_type = self.options.build_type

        # Handle OptimizedDebug special case
        build_type_is_optimizeddebug = build_type.lower() == 'optimizeddebug'
        cmake_build_type = "Debug" if build_type_is_optimizeddebug else build_type

        if build_type:
            config_args.append(f"-DCMAKE_BUILD_TYPE={cmake_build_type}")
            if build_type_is_optimizeddebug:
                if is_windows():
                    cxx_flags += " /DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"
                    cc_flags += " /DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"
                else:
                    cxx_flags += " -Og -g"
                    cc_flags += " -Og -g"

        config_args.extend(extra_args)

        if cc_flags.strip():
            config_args.append(f"-DCMAKE_C_FLAGS={cc_flags.strip()}")
        if cxx_flags.strip():
            config_args.append(f"-DCMAKE_CXX_FLAGS={cxx_flags.strip()}")
        if ld_flags.strip():
            config_args.append(f"-DCMAKE_EXE_LINKER_FLAGS={ld_flags.strip()}")
            config_args.append(f"-DCMAKE_SHARED_LINKER_FLAGS={ld_flags.strip()}")

        # Configure
        self.ui.info("Configuring...")
        run_cmd(
            config_args,
            cwd=self.options.source_dir,
            tail=True,
            dry_run=self.options.dry_run,
            verbose=self.options.verbose,
            debug=self.options.debug,
            env=self.env,
            ui=self.ui,
        )

        # Build
        self.ui.info("Building...")
        build_args = [cmake_exe, "--build", build_dir, "--config", cmake_build_type]
        parallel_level = max(1, os.cpu_count() or 1)
        build_args.extend(["--parallel", str(parallel_level)])
        run_cmd(
            build_args,
            cwd=self.options.source_dir,
            tail=True,
            dry_run=self.options.dry_run,
            verbose=self.options.verbose,
            debug=self.options.debug,
            env=self.env,
            ui=self.ui,
        )

        # Install
        self.ui.info("Installing...")
        install_args = [cmake_exe, "--install", build_dir]
        if install_dir:
            install_args.extend(["--prefix", install_dir])
        if cmake_build_type:
            install_args.extend(["--config", cmake_build_type])
        run_cmd(
            install_args,
            cwd=self.options.source_dir,
            tail=True,
            dry_run=self.options.dry_run,
            verbose=self.options.verbose,
            debug=self.options.debug,
            env=self.env,
            ui=self.ui,
        )

    def _resolve_build_install_dirs(self):
        """Expand and set build_dir/install_dir from their placeholders.

        Idempotent: only fills values that are not already set. Run configs
        are generated before MrDocs is built, so the run-config generator
        relies on this having been called first; otherwise build_dir is empty
        and preset-derived paths (e.g. `--stdlib-includes`) collapse.
        """
        build_dir = self._expand_placeholders(self.default_options.build_dir)
        install_dir = self._expand_placeholders(self.default_options.install_dir)

        if not self.options.build_dir:
            self.options.build_dir = build_dir
        if not self.options.system_install and not self.options.install_dir:
            self.options.install_dir = install_dir

    def install_mrdocs(self):
        """Configure, build, and install MrDocs."""
        if self.options.skip_build:
            self.ui.info("Skipping MrDocs build (--skip-build specified).")
            return

        # Check and repair git symlinks (important on Windows)
        check_git_symlinks(
            self.options.source_dir,
            git_path=self.options.git_path or "git",
            dry_run=self.options.dry_run,
            ui=self.ui,
        )

        self._resolve_build_install_dirs()

        extra_args = []
        if not self.options.system_install and self.options.install_dir:
            extra_args.append(f"-DCMAKE_INSTALL_PREFIX={self.options.install_dir}")

        extra_args.append(f"--preset={self.options.preset}")

        # Handle DebugFast -> Debug mapping
        main_build_type = "Debug" if self.options.build_type.lower() in ("debugfast", "debug-fast") else self.options.build_type

        self.cmake_workflow(
            self.options.source_dir,
            main_build_type,
            self.options.build_dir,
            self.options.install_dir,
            extra_args,
            cc_flags=self.options.cflags,
            cxx_flags=self.options.cxxflags,
            ld_flags=self.options.ldflags,
            force_rebuild=False,
            remove_build_dir=False,
            allow_skip=False,
        )

        self.ui.ok(f"MrDocs installed to {self.ui.maybe_shorten(self.options.install_dir)}")
        self.print_mrdocs_summary()

    def run_mrdocs_tests(self):
        """Run MrDocs tests with ctest."""
        if not self.options.build_tests:
            self.ui.warn("Tests were not built (--no-build-tests). Skipping test run.")
            return

        build_dir = self._expand_placeholders(self.default_options.build_dir)
        cmake_exe = self.options.cmake_path or "cmake"

        # Find ctest relative to cmake
        ctest_path = os.path.join(os.path.dirname(cmake_exe), "ctest")
        if is_windows():
            ctest_path += ".exe"
        if not os.path.exists(ctest_path):
            # Try finding ctest in PATH
            import shutil
            ctest_path = shutil.which("ctest")
            if not ctest_path:
                raise FileNotFoundError("ctest executable not found. Please ensure CMake is installed correctly.")

        self.ui.info("Running tests...")
        test_args = [
            ctest_path,
            "--test-dir", build_dir,
            "--output-on-failure",
            "--progress",
            "--no-tests=error",
            "--parallel", str(os.cpu_count() or 1),
        ]
        run_cmd(
            test_args,
            cwd=self.options.source_dir,
            tail=True,
            dry_run=self.options.dry_run,
            verbose=self.options.verbose,
            debug=self.options.debug,
            env=self.env,
            ui=self.ui,
        )

        self.ui.ok("All tests passed.")

    def refresh_all(self):
        """
        Refresh all existing bootstrap configurations.

        Reads the IDE launch configurations (.vscode/launch.json or .vs/launch.vs.json)
        and re-runs bootstrap with those saved arguments.
        """
        import json as json_module
        import shlex
        import subprocess
        import sys

        current_python_interpreter_path = sys.executable
        source_dir = self.options.source_dir
        vscode_launch_path = os.path.join(source_dir, ".vscode", "launch.json")
        vs_launch_path = os.path.join(source_dir, ".vs", "launch.vs.json")
        use_vscode = os.path.exists(vscode_launch_path)
        use_vs = os.path.exists(vs_launch_path)

        if not use_vscode and not use_vs:
            self.ui.info("No existing refresh launch configurations found.")
            return

        if use_vscode:
            with open(vscode_launch_path, "r") as f:
                launch_data = json_module.load(f)
            configs = launch_data.get("configurations", [])
        else:
            with open(vs_launch_path, "r") as f:
                launch_data = json_module.load(f)
            configs = launch_data.get("configurations", [])

        # Filter configurations for bootstrap refresh
        bootstrap_refresh_configs = [
            cfg for cfg in configs if
            cfg.get("name", "").startswith("Bootstrap Refresh (") and cfg.get("name", "").endswith(")")
        ]

        if not bootstrap_refresh_configs:
            self.ui.info("No bootstrap refresh configurations found in IDE launch configurations.")
            return

        # Find bootstrap.py path
        bootstrap_script = os.path.join(source_dir, "bootstrap.py")
        if not os.path.exists(bootstrap_script):
            # Try running as module
            bootstrap_script = None

        # Saved configurations can go stale across bootstrap versions (a flag
        # that existed when they were written may have since been removed).
        # Validate each config's args against the current parser, drop any it
        # no longer recognizes, and keep going if one config still fails, so a
        # single stale entry never aborts the whole refresh.
        from . import __main__ as bootstrap_main
        parser = bootstrap_main.build_arg_parser()

        for config in bootstrap_refresh_configs:
            config_name = config['name']
            if use_vscode:
                args = [arg.replace("${workspaceFolder}", source_dir) for arg in config.get("args", [])]
            else:
                args = shlex.split(config.get("scriptArguments", ""))

            try:
                _, unknown = parser.parse_known_args(args)
            except SystemExit:
                unknown = []
            if unknown:
                dropped = set(unknown)
                args = [a for a in args if a not in dropped]
                self.ui.info(
                    f"  (ignoring arguments no longer recognized by bootstrap: "
                    f"{' '.join(sorted(dropped))})"
                )

            self.ui.info(f"Refreshing configuration '{config_name}':")
            for arg in args:
                self.ui.info(f"  * {arg}")

            if bootstrap_script:
                cmd = [current_python_interpreter_path, bootstrap_script] + args
            else:
                cmd = [current_python_interpreter_path, "-m", "util.bootstrap"] + args
            try:
                subprocess.run(cmd, check=True)
            except subprocess.CalledProcessError as exc:
                self.ui.info(
                    f"Configuration '{config_name}' failed (exit {exc.returncode}); "
                    f"continuing with the rest."
                )

    def _dry_comment(self, text: str):
        """Print a shell comment to stdout when in dry-run mode.

        These comments appear inline with the copy-pasteable commands,
        giving users context about what each block of commands does.
        """
        if self.options.dry_run:
            print(f"\n# {text}")

    def _dry_preamble(self):
        """Print a shell script preamble in dry-run mode."""
        if not self.options.dry_run:
            return
        print("#!/usr/bin/env bash")
        print("set -euo pipefail")
        print()
        print("# MrDocs bootstrap - equivalent manual steps")
        print(f"# Generated for: {get_os_name()}")
        print(f"# Source directory: {self.options.source_dir}")

    def _dry_config_summary(self):
        """Print resolved configuration as exported shell variables.

        Called after all prompts/tool detection are done so values are final.
        Variables are exported so the script is self-contained and users can
        reference them (e.g. $CC, $CMAKE) when adapting commands.
        """
        if not self.options.dry_run:
            return
        print()
        print("# --- Resolved configuration ---")
        if self.options.cc:
            print(f"export CC={_shquote(self.options.cc)}")
        if self.options.cxx:
            print(f"export CXX={_shquote(self.options.cxx)}")
        if self.options.cmake_path:
            print(f"export CMAKE={_shquote(self.options.cmake_path)}")
        if self.options.ninja_path:
            print(f"export NINJA={_shquote(self.options.ninja_path)}")
        if self.options.git_path:
            print(f"export GIT={_shquote(self.options.git_path)}")
        print(f"export BUILD_TYPE={_shquote(self.options.build_type or 'Release')}")
        print(f"export PRESET={_shquote(self.options.preset or 'default')}")
        if self.options.sanitizer:
            print(f"export SANITIZER={_shquote(self.options.sanitizer)}")
        if self.options.cflags:
            print(f"export CFLAGS={_shquote(self.options.cflags)}")
        if self.options.cxxflags:
            print(f"export CXXFLAGS={_shquote(self.options.cxxflags)}")
        if self.options.ldflags:
            print(f"export LDFLAGS={_shquote(self.options.ldflags)}")
        print("# --- End configuration ---")

    def run(self):
        """Run the complete bootstrap process."""
        self.ui.section("MrDocs Bootstrap")
        self._dry_preamble()

        # On Windows, probe MSVC development environment first
        if is_windows():
            msvc_env = probe_msvc_dev_env()
            if msvc_env:
                self.env.update(msvc_env)

        # Print environment variable exports for dry-run
        self._dry_comment("Environment setup")
        if self.options.dry_run:
            print("export PKG_CONFIG=false")
            cpu_count = max(1, os.cpu_count() or 1)
            print(f"export CMAKE_BUILD_PARALLEL_LEVEL={cpu_count}")

        # Phase 0: Check system prerequisites
        self.ui.subsection("Checking system prerequisites")
        self._dry_comment("Phase 0: Check system prerequisites (cmake, git, python3, cc, java)")
        self.check_system_prerequisites()

        # Phase 1: Check tools
        self.ui.subsection("Checking tools")
        self.check_tools()

        # Phase 2: Setup compilers
        self.ui.subsection("Setting up compilers")
        self.setup_compilers()

        # Phase 3: Build options
        self.ui.subsection("Build configuration")
        self.setup_build_options()
        self.prompt_option("preset", "CMake preset")

        # Phase 4: Probe compilers
        if self.options.cc or self.options.cxx:
            self.ui.subsection("Probing compilers")
            self._dry_comment(f"Probe compilers: cc={self.options.cc or 'default'} cxx={self.options.cxx or 'default'}")
            probe_dir = os.path.join(self.options.third_party_src_dir, "cmake-probe")
            self.compiler_info = probe_compilers(
                self.options.cmake_path,
                probe_dir,
                self.options.cc,
                self.options.cxx,
                self.options.dry_run,
                self.ui,
            )

        # Phase 5: Setup Ninja
        self.ui.subsection("Setting up Ninja")
        self._dry_comment("Setup Ninja build system")
        self.setup_ninja()

        # Print resolved configuration summary for dry-run
        self._dry_config_summary()

        # Phase 6: Install dependencies (each dependency gets its own section header)
        self._dry_comment("Install third-party dependencies")
        self.install_dependencies()

        # Write CI env file (after deps are installed, before MrDocs build)
        self.write_env_file()

        # Phase 7: MrDocs configuration and build
        self.ui.section("MrDocs")

        # Create presets
        self.ui.subsection("Creating CMake presets")
        self._dry_comment("Generate CMakeUserPresets.json")
        self.create_presets()
        presets_path = os.path.join(self.options.source_dir, "CMakeUserPresets.json")
        self.ui.ok(f"CMake presets written to {self.ui.maybe_shorten(presets_path)}")
        if not self.options.dry_run:
            self.show_preset_summary()

        # Generate IDE configs
        if self.options.generate_run_configs:
            # Run configs reference build_dir/install_dir, which are only
            # expanded during the build step below; resolve them first so
            # preset-derived paths (e.g. --stdlib-includes) are correct.
            self._resolve_build_install_dirs()
            self.ui.subsection("Generating IDE configurations")
            self._dry_comment("Generate IDE run configurations")
            self.generate_configs()
            configs_generated = []
            if self.options.generate_clion_run_configs:
                configs_generated.append("CLion")
            if self.options.generate_vscode_run_configs:
                configs_generated.append("VSCode")
            if self.options.generate_vs_run_configs:
                configs_generated.append("Visual Studio")
            if configs_generated:
                self.ui.ok(f"IDE run configurations generated: {', '.join(configs_generated)}")

            if self.options.generate_pretty_printer_configs:
                self.ui.ok("Pretty printer configurations generated.")

        # Build and install MrDocs
        self.ui.subsection("Building MrDocs")
        self._dry_comment("Configure, build, and install MrDocs")
        self.install_mrdocs()

        # Run tests
        if self.options.run_tests:
            self.ui.subsection("Running tests")
            self._dry_comment("Run MrDocs tests")
            self.run_mrdocs_tests()

        self.ui.ok("Bootstrap complete!")
        self.ui.end_group()
