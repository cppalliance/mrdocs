#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
"""
Configure, build, and run all MrDocs tests with sensible preset selection.
"""

import argparse
import json
import os
import platform
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional


ROOT = Path(__file__).resolve().parent.parent
PRESETS_FILE = ROOT / "CMakeUserPresets.json"

def error(message: str) -> None:
    """Print an error message to stderr without exiting."""
    print(f"error: {message}", file=sys.stderr)


def load_presets() -> Dict[str, dict]:
    """Load CMakeUserPresets.json and return a name->preset map."""
    if not PRESETS_FILE.exists():
        error(
            "CMake user presets are missing. Run bootstrap.py to generate "
            "CMakeUserPresets.json."
        )
        sys.exit(1)
    try:
        data = json.loads(PRESETS_FILE.read_text())
    except json.JSONDecodeError as exc:
        error(f"Could not parse {PRESETS_FILE}: {exc}")
        sys.exit(1)

    presets = {preset["name"]: preset for preset in data.get("configurePresets", [])}
    if not presets:
        error("No configurePresets entries found in CMakeUserPresets.json.")
        sys.exit(1)
    return presets


BUILD_TYPES = {"release", "debug", "relwithdebinfo", "minsizerel", "optimizeddebug"}
OS_ALIASES = {
    "darwin": "macos",
    "macos": "macos",
    "osx": "macos",
    "linux": "linux",
    "wsl": "linux",
    "windows": "windows",
    "win": "windows",
}
COMPILER_TOKENS = {
    "appleclang": "clang",
    "apple-clang": "clang",
    "clang": "clang",
    "gcc": "gcc",
    "g++": "gcc",
    "msvc": "msvc",
    "cl": "msvc",
}


def parse_preset_name(name: str) -> tuple[Optional[str], Optional[str], Optional[str]]:
    """
    Extract build type, OS, and compiler tokens from a preset name.

    Returns:
        (build_type | None, os | None, compiler | None)
    """
    tokens = name.lower().replace("/", "-").split("-")
    build = next((t for t in tokens if t in BUILD_TYPES), None)
    os_token = next((OS_ALIASES[t] for t in tokens if t in OS_ALIASES), None)
    compiler = next((COMPILER_TOKENS[t] for t in tokens if t in COMPILER_TOKENS), None)
    return build, os_token, compiler


def preset_score(
    name: str, preset: dict, host_os: str
) -> tuple[int, int, int, int, int, int, int, int]:
    """
    Compute a priority score for a preset based on desirability signals.

    Higher tuples win; each position reflects:
        has_llvm, is_release, has_build, os_matches, has_os,
        prefers_default_compiler, compiler_absent_flag, name_shortness_hint
    """
    build, os_token, compiler = parse_preset_name(name)
    cache_vars = preset.get("cacheVariables", {}) or {}
    # Reward presets that declare an LLVM path
    has_llvm = int(any(k.lower() == "llvm_root" for k in cache_vars))
    # Reward presets that specify any build type
    has_build = int(build is not None)
    # Prefer Release over other build types
    is_release = int(build == "release")
    # Reward presets that declare an OS token
    has_os = int(os_token is not None)
    # Extra reward when the OS token matches the host
    os_matches = int(os_token == host_os)
    # Track whether a compiler was explicitly named
    compiler_present = int(compiler is not None)
    # Prefer presets that leave compiler unspecified (use default)
    prefers_default_compiler = int(not compiler_present)
    # Slightly prefer shorter names when all else is equal
    name_penalty = -len(name)
    return (
        has_llvm,
        is_release,
        has_build,
        os_matches,
        has_os,
        prefers_default_compiler,
        -compiler_present,
        name_penalty,
    )


def pick_preset(presets: Dict[str, dict], override: Optional[str]) -> str:
    """
    Choose the best preset for the current host (or honor an explicit override).

    Selection is based on parsed build/OS/compiler tokens and signals like
    release build, host OS match, absence of custom compiler, and presence of
    an LLVM_ROOT cache variable.
    """
    if override:
        return override

    host = platform.system()
    host_os = "macos" if host == "Darwin" else host.lower()

    # Scan every preset and keep the one with the highest score.
    best_name: Optional[str] = None
    best_score: Optional[tuple[int, int, int, int, int, int, int, int]] = None

    for name, preset in presets.items():
        score = preset_score(name, preset, host_os)
        if best_score is None or score > best_score:
            best_score = score
            best_name = name

    if best_name:
        return best_name

    error(
        "Could not find a suitable preset. "
        "Run bootstrap.py to regenerate presets or provide --preset explicitly."
    )
    sys.exit(1)


def build_dir_for(preset_name: str) -> Path:
    """Return the build directory path for a given preset name."""
    return ROOT / "build" / preset_name


def install_dir_for(preset_name: str) -> Path:
    """Return the install prefix path for a given preset name."""
    return ROOT / "install" / preset_name


def run_command(
    command: List[str],
    cwd: Optional[Path] = None,
    env: Optional[dict] = None,
) -> None:
    """
    Run a command, printing it first and raising on failure.

    If env is provided, it is merged over the current environment.
    """
    print(f"> {' '.join(command)}")
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    completed = subprocess.run(command, cwd=cwd or ROOT, env=merged_env)
    if completed.returncode != 0:
        raise subprocess.CalledProcessError(completed.returncode, command)


def run_ctest(build_dir: Path, regex: Optional[str] = None, exclude: Optional[str] = None) -> int:
    """
    Run ctest in a build directory, optionally filtering by regex or exclusion.

    Returns:
        ctest exit code
    """
    command = ["ctest", "--test-dir", str(build_dir), "--output-on-failure"]
    if regex:
        command.extend(["-R", regex])
    if exclude:
        command.extend(["-E", exclude])
    print(f"> {' '.join(command)}")
    result = subprocess.run(command, cwd=build_dir)
    return result.returncode


def run_ctest_case(build_dir: Path, name: str) -> int:
    """Run a single-named ctest case by exact match and return its exit code."""
    command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "--output-on-failure",
        "-R",
        f"^{name}$",
    ]
    print(f"> {' '.join(command)}")
    result = subprocess.run(command, cwd=build_dir)
    return result.returncode


def list_ctest_names(build_dir: Path) -> List[str]:
    """Return the list of test names discovered by `ctest -N`."""
    proc = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "-N"],
        text=True,
        capture_output=True,
        cwd=build_dir,
    )
    if proc.returncode != 0:
        return []
    names: List[str] = []
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("Test #"):
            parts = line.split(": ", 1)
            if len(parts) == 2:
                names.append(parts[1])
    return names or []


def append_missing_flags(flags: Optional[str], required: List[str]) -> str:
    """Append required flags if they are not already present."""
    tokens = (flags.split() if flags else [])
    for flag in required:
        if flag not in tokens:
            tokens.append(flag)
    return " ".join(tokens)


def infer_warning_flags(preset_name: str, preset: Optional[dict]) -> List[str]:
    """Choose the appropriate warning-as-error + level flags for the toolchain."""
    _, _, compiler_token = parse_preset_name(preset_name)
    compiler_hint = compiler_token

    if preset:
        cache_vars = preset.get("cacheVariables", {}) or {}
        cxx_compiler = str(cache_vars.get("CMAKE_CXX_COMPILER", "")).lower()
        if cxx_compiler:
            if "clang" in cxx_compiler:
                compiler_hint = "clang"
            elif "g++" in cxx_compiler or "gcc" in cxx_compiler:
                compiler_hint = "gcc"
            elif "cl" in cxx_compiler:
                compiler_hint = "msvc"

    if compiler_hint == "msvc" or platform.system() == "Windows":
        return ["/WX", "/W4"]
    return ["-Werror", "-Wall"]


def main() -> None:
    """CLI entry point: choose preset, configure, build, and run tests."""
    parser = argparse.ArgumentParser(
        description="Configure, build, and run all MrDocs tests."
    )
    parser.add_argument(
        "--preset",
        help="Configure/build preset to use (defaults to a release/* preset for this OS).",
    )
    parser.add_argument(
        "--update-golden",
        action="store_true",
        help="Refresh golden tests before running the test suite.",
    )
    parser.add_argument(
        "--skip-docs",
        action="store_true",
        help="Skip building documentation.",
    )
    parser.add_argument(
        "--no-strict",
        action="store_true",
        help="Disable MRDOCS_BUILD_STRICT_TESTS.",
    )
    parser.add_argument(
        "--parallel",
        type=int,
        default=os.cpu_count() or 1,
        help="Number of parallel jobs for cmake --build (default: CPU count).",
    )
    args = parser.parse_args()

    presets: Dict[str, dict] = {}
    preset_name: str
    if args.preset:
        preset_name = args.preset
        if PRESETS_FILE.exists():
            presets = load_presets()
    else:
        presets = load_presets()
        preset_name = pick_preset(presets, None)
    build_dir = build_dir_for(preset_name)
    install_dir = install_dir_for(preset_name)

    build_docs = not args.skip_docs

    strict_tests = not args.no_strict

    preset = presets.get(preset_name)
    warning_flags = infer_warning_flags(preset_name, preset)
    cache_vars = preset.get("cacheVariables", {}) if preset else {}
    c_flags = append_missing_flags(
        cache_vars.get("CMAKE_C_FLAGS") or os.environ.get("CFLAGS"), warning_flags
    )
    cxx_flags = append_missing_flags(
        cache_vars.get("CMAKE_CXX_FLAGS") or os.environ.get("CXXFLAGS"), warning_flags
    )
    try:
        # Configure and build using the chosen preset.
        configure_cmd = ["cmake", "--preset", preset_name]
        configure_cmd.append(f"-DMRDOCS_BUILD_STRICT_TESTS={'ON' if strict_tests else 'OFF'}")
        configure_cmd.append(f"-DCMAKE_C_FLAGS={c_flags}")
        configure_cmd.append(f"-DCMAKE_CXX_FLAGS={cxx_flags}")
        run_command(configure_cmd)
        run_command(["cmake", "--build", str(build_dir), "--parallel", str(args.parallel)])

        if args.update_golden:
            print(
                "Updating golden tests before running ctest. "
                "Only do this after confirming output diffs are intentional."
            )
            run_command(
                [
                    "cmake",
                    "--build",
                    str(build_dir),
                    "--target",
                    "mrdocs-update-test-fixtures-all",
                    "--parallel",
                    str(args.parallel),
                ]
            )

        # Discover all tests up front so we can split golden from everything else.
        tests = list_ctest_names(build_dir)
        golden_tests = [t for t in tests if "golden" in t]
        non_golden_tests = [t for t in tests if "golden" not in t]

        # Run golden tests first to give targeted guidance.
        golden_rc = 0
        if golden_tests:
            golden_rc = run_ctest(build_dir, regex="mrdocs-golden-tests")
            if golden_rc != 0:
                error("Golden tests failed.")
                print(
                    "If the failures are only golden output differences, review them for "
                    "intentionality. If AND ONLY IF they are intentional, rerun with --update-golden "
                    "to refresh the fixtures."
                )
                sys.exit(golden_rc)

        # Run non-golden tests one by one with targeted suggestions.
        fixups = {
            "mrdocs-unit-tests": (
                "Unit tests failed. Rerun with `ctest --test-dir {build} -V -R mrdocs-unit-tests` "
                "to see the exact failures, fix the code/test, then rerun."
            ),
            "xml-lint": (
                "XML lint failed. Ensure libxml2 is installed and on PATH. "
                "Reproduce with `ctest --test-dir {build} -V -R xml-lint`."
            ),
            "yaml-schema-check": (
                "YAML schema check failed. Run `python util/generate-yaml-schema.py --check` from the repo root, "
                "update ConfigOptions.json / generated schema if needed, then rerun "
                "`ctest --test-dir {build} -V -R yaml-schema-check`."
            ),
            "mrdocs-self-doc": (
                "Self-doc run failed. Rerun with `ctest --test-dir {build} -V -R mrdocs-self-doc` "
                "to inspect mrdocs output. Common issues: wrong stdlib/libc include paths, "
                "doc warnings promoted to errors when MRDOCS_BUILD_STRICT_TESTS=ON, or stale docs/mrdocs.yml config."
            ),
        }
        for name in non_golden_tests or ["mrdocs-unit-tests", "xml-lint", "yaml-schema-check", "mrdocs-self-doc"]:
            rc = run_ctest_case(build_dir, name)
            if rc != 0:
                error(f"Test failed: {name}")
                msg = fixups.get(name)
                if msg:
                    # allow build dir substitution when present
                    print(msg.format(build=str(build_dir)))
                else:
                    print(
                        f"Inspect detailed output with `ctest --test-dir {build_dir} -V -R {name}`; "
                        f"additional logs: `{build_dir}/Testing/Temporary/LastTest.log`. "
                        "Address the reported failure, then rerun the same ctest command or this script."
                    )
                sys.exit(rc)

        # Install into a scoped prefix under the repository root to avoid polluting the system.
        install_dir.mkdir(parents=True, exist_ok=True)
        run_command(["cmake", "--install", str(build_dir), "--prefix", str(install_dir)])

        # Optionally build docs via the repository scripts instead of CMake.
        if build_docs:
            docs_dir = ROOT / "docs"
            build_sh = docs_dir / "build_local_docs.sh"
            build_bat = docs_dir / "build_local_docs.bat"
            # Pick the script that matches the host OS; both files may exist.
            if platform.system() == "Windows" and build_bat.exists():
                run_command([str(build_bat)], cwd=docs_dir, env={"MRDOCS_ROOT": str(install_dir)})
            elif platform.system() != "Windows" and build_sh.exists():
                # Call explicitly via bash to avoid exec-format errors when the script lacks a shebang.
                run_command(["bash", str(build_sh)], cwd=docs_dir, env={"MRDOCS_ROOT": str(install_dir)})
            else:
                error("No docs build script found (expected docs/build_local_docs.sh or docs/build_local_docs.bat).")

    except FileNotFoundError as exc:
        error(f"Required tool not found: {exc}")
        sys.exit(1)
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)


if __name__ == "__main__":
    main()
