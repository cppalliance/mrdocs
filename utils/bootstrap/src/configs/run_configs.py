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
Run configuration generation orchestration.

Coordinates generation of run configurations for all supported IDEs.
"""

import dataclasses
import os
import re
from typing import Optional, List, Dict, Any

from ..core.platform import is_windows
from ..core.filesystem import load_json_file
from ..core.ui import TextUI, get_default_ui
from ..core.options import InstallOptions
from .cmake_file_api import CMakeReplyError, target_artifact


def resolve_target_programs(configs: List[Dict[str, Any]], build_dir: str,
                            build_type: Optional[str] = None,
                            dry_run: bool = False) -> None:
    """Set ``program`` on every target entry from CMake's file API reply.

    The path is the one CMake reports for the executable of the entry's
    ``target`` in ``build_dir``. An entry whose target has no reply raises a
    ``ValueError`` naming the entry, so a missing program is an error when the
    configs are generated and never a recipe that fails to start. A dry run
    writes nothing, so a missing reply leaves the entry without a program.
    """
    for cfg in configs:
        if "target" not in cfg:
            continue
        try:
            cfg["program"] = os.path.join(
                build_dir, target_artifact(build_dir, cfg["target"], build_type))
        except CMakeReplyError as e:
            if dry_run:
                continue
            raise ValueError(f"run config {cfg.get('name')!r}: {e}") from e


def _cli_flag_for_dest() -> Dict[str, str]:
    """Map each argparse `dest` to the first CLI option string the
    parser accepts (preferring long forms over short ones). Used to
    convert `InstallOptions` field names into CLI flags that the
    bootstrap actually recognizes, instead of blindly turning
    `field_name` into `--field-name` (which breaks for fields whose
    `dest` differs from their flag, like `non_interactive` ↔ `--yes`
    or `plain_ui` ↔ `--plain`)."""
    from .. import __main__ as bootstrap_main  # avoid import cycle at module load
    parser = bootstrap_main.build_arg_parser()
    mapping: Dict[str, str] = {}
    for action in parser._actions:
        if not action.option_strings or action.dest in ("help", "version"):
            continue
        long_flags = [s for s in action.option_strings if s.startswith("--")]
        mapping[action.dest] = (long_flags or action.option_strings)[0]
    return mapping

# Variable expansion pattern for $var and ${var} syntax
_VAR_PATTERN = re.compile(r"\$(\w+)|\${([^}]+)}")

# A generated per-preset entry name looks like "<base> (<preset>)".
_PRESET_SUFFIX = re.compile(r"^(.*) \(([^()]+)\)$")


def is_stale_preset_entry(name, generated_bases, valid_presets) -> bool:
    """True when ``name`` is a generator-owned per-preset entry whose preset
    is no longer present.

    An entry is pruned only when its base name (the part before the trailing
    " (preset)") is one we still generate AND the preset token is not in the
    current preset set. This never matches user-authored entries, whose base
    names are not in ``generated_bases``.
    """
    if not name:
        return False
    m = _PRESET_SUFFIX.match(name)
    if not m:
        return False
    base, token = m.group(1), m.group(2)
    return base in generated_bases and token not in valid_presets


def is_stale_generated_entry(name, marked, current_names, generated_bases, valid_presets) -> bool:
    """True when ``name`` is an entry the generator wrote earlier that the
    current run no longer produces.

    Only ``marked`` entries (those carrying the generator's own marker) are
    candidates, so an entry the user wrote or edited by hand survives whatever
    its name. A marked entry stays when this run produced it (``current_names``)
    or when it is the per-preset entry of another preset still present.
    """
    if not marked or name in current_names:
        return False
    m = _PRESET_SUFFIX.match(name or "")
    if m and m.group(1) in generated_bases and m.group(2) in valid_presets:
        return False
    return True


def load_preset_names(source_dir: str) -> list:
    """Read configure preset names from CMakeUserPresets.json (best effort)."""
    path = os.path.join(source_dir, "CMakeUserPresets.json")
    data = load_json_file(path) or {}
    return [p.get("name") for p in data.get("configurePresets", []) if p.get("name")]


def _read_cmake_cache_var(build_dir: str, var: str) -> str:
    """Return a variable's value from a build's CMakeCache.txt, or ''."""
    path = os.path.join(build_dir, "CMakeCache.txt")
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                key = line.split("=", 1)[0].split(":", 1)[0].strip()
                if key == var and "=" in line:
                    return line.split("=", 1)[1].strip()
    except OSError:
        pass
    return ""


def stdlib_includes_dir(source_dir: str, build_dir: str) -> str:
    """Resolve the libc++ include directory mrdocs needs (`--stdlib-includes`).

    Prefer the configured `LIBCXX_DIR` from the build's CMakeCache.txt (the
    exact value the golden-test ctest targets use); fall back to the documented
    third-party install convention so a recipe still has a path before the
    build is configured.
    """
    cached = _read_cmake_cache_var(build_dir, "LIBCXX_DIR")
    if cached:
        return cached
    preset = os.path.basename(build_dir.rstrip("/\\"))
    return os.path.join(source_dir, "build", "third-party", "install", preset,
                        "llvm", "include", "c++", "v1")


def expand_with(s: str, mapping: Dict[str, Any]) -> str:
    """Replace $var and ${var} placeholders in a string using mapping."""
    def repl(m):
        key = m.group(1) or m.group(2)
        return str(mapping.get(key, m.group(0)))
    return _VAR_PATTERN.sub(repl, s)


def format_values(obj, tokens: Dict[str, Any]):
    """Recursively expand $var placeholders in strings, lists, and dicts."""
    if isinstance(obj, str):
        return expand_with(obj, tokens)
    if isinstance(obj, list):
        return [format_values(x, tokens) for x in obj]
    if isinstance(obj, dict):
        return {k: format_values(v, tokens) for k, v in obj.items()}
    return obj


_PATH_FLAGS_FOR_SAVED_ARGS = ("--preset", "--build-dir", "--install-dir")


def load_saved_bootstrap_args(source_dir: str) -> Dict[str, List[str]]:
    """
    Read the arguments saved in the IDE launch files for the bootstrap entries.

    Looks at `.vscode/launch.json` first and `.vs/launch.vs.json` second and
    returns a mapping from entry name (`Bootstrap Update (<preset>)` and
    `Bootstrap Refresh (<preset>)`) to its argument list, with
    `${workspaceFolder}` expanded to `source_dir`.
    """
    import json
    import shlex

    saved: Dict[str, List[str]] = {}
    vscode_path = os.path.join(source_dir, ".vscode", "launch.json")
    vs_path = os.path.join(source_dir, ".vs", "launch.vs.json")
    for path, is_vscode in ((vs_path, False), (vscode_path, True)):
        if not os.path.isfile(path):
            continue
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            continue
        for cfg in data.get("configurations", []):
            name = cfg.get("name", "")
            if not (name.startswith("Bootstrap Update (") or name.startswith("Bootstrap Refresh (")):
                continue
            if not name.endswith(")"):
                continue
            if is_vscode:
                raw = cfg.get("args")
                if not isinstance(raw, list):
                    continue
                args = [str(a).replace("${workspaceFolder}", source_dir) for a in raw]
            else:
                text = cfg.get("scriptArguments")
                if not isinstance(text, str):
                    continue
                args = shlex.split(text)
            saved[name] = args
    return saved


def _merge_saved_args(saved: List[str], generated: List[str]) -> List[str]:
    """Reuse the saved arguments, adding --preset/--build-dir/--install-dir only when missing."""
    merged = list(saved)
    for flag in _PATH_FLAGS_FOR_SAVED_ARGS:
        if flag in merged:
            continue
        for i, arg in enumerate(generated[:-1]):
            if arg == flag:
                merged.extend([flag, generated[i + 1]])
                break
    return merged


def get_dynamic_run_configs(
    options: InstallOptions,
    default_options: InstallOptions,
    java_path: str = "",
    saved_args: Optional[Dict[str, List[str]]] = None,
) -> List[Dict[str, Any]]:
    """
    Generate dynamic run configuration data.

    Creates configuration entries for bootstrap helpers, Boost documentation
    targets, and the schema generation task.

    Args:
        options: Current install options.
        default_options: Default install options (for comparison).
        java_path: Path to Java executable.
        saved_args: Arguments already saved in the IDE files for the
            `Bootstrap Update (...)` and `Bootstrap Refresh (...)` entries,
            keyed by entry name. When an entry is present its arguments are
            reused instead of the ones derived from `options`.

    Returns:
        List of run configuration dictionaries.
    """
    configs: List[Dict[str, Any]] = []

    # Bootstrap helper targets. Only InstallOptions fields that have a
    # corresponding CLI flag are emitted; the rest are programmatic-only
    # (e.g. generate_vs_run_configs, third_party_src_dir) and would make
    # bootstrap exit with `unrecognized arguments` if passed to it.
    cli_flag_for_dest = _cli_flag_for_dest()
    bootstrap_args: List[str] = []
    for field in dataclasses.fields(InstallOptions):
        if field.name == "non_interactive":
            continue
        flag = cli_flag_for_dest.get(field.name)
        if flag is None:
            continue
        value = getattr(options, field.name)
        default_value = getattr(default_options, field.name, None)
        if value is None or (value == default_value and field.name != "build_type"):
            continue
        if field.type is bool:
            # `store_false` actions are spelled as a negative flag
            # (`--no-build-tests`, `--no-run-configs`) and should be
            # emitted only when the option is False (i.e. the user
            # opted out of the True default). Plain `store_true`
            # actions are spelled as a positive flag and should be
            # emitted only when the option is True.
            if flag.startswith("--no-"):
                if not value:
                    bootstrap_args.append(flag)
            else:
                if value:
                    bootstrap_args.append(flag)
        elif field.type is str:
            if value != "":
                bootstrap_args.append(flag)
                bootstrap_args.append(value)
        else:
            raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")

    bootstrap_refresh_config_name = options.preset or options.build_type or "debug"
    update_name = f"Bootstrap Update ({bootstrap_refresh_config_name})"
    refresh_name = f"Bootstrap Refresh ({bootstrap_refresh_config_name})"
    update_args = list(bootstrap_args)
    refresh_args = bootstrap_args + ["--non-interactive"]
    if saved_args:
        if update_name in saved_args:
            update_args = _merge_saved_args(saved_args[update_name], bootstrap_args)
        if refresh_name in saved_args:
            refresh_args = _merge_saved_args(saved_args[refresh_name], bootstrap_args)
            if "--non-interactive" not in refresh_args:
                refresh_args.append("--non-interactive")

    configs.extend([
        {
            "name": "Bootstrap Help",
            "group": "Bootstrap",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "args": ["--help"],
            "cwd": options.source_dir
        },
        {
            "name": update_name,
            "group": "Bootstrap",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "folder": "Bootstrap Update",
            "args": update_args,
            "cwd": options.source_dir
        },
        {
            "name": refresh_name,
            "group": "Bootstrap",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "folder": "Bootstrap Refresh",
            "args": refresh_args,
            "cwd": options.source_dir
        },
        {
            "name": "Bootstrap Refresh All",
            "group": "Bootstrap",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "folder": "Bootstrap Refresh",
            "args": ["--refresh-all"],
            "cwd": options.source_dir
        },
        {
            "name": f"Generate Config Info ({bootstrap_refresh_config_name})",
            "group": "Codegen",
            "script": os.path.join(options.source_dir, "utils", "codegen", "generate-config-info.py"),
            "folder": "Generate Config Info",
            "args": [
                os.path.join(options.source_dir, "src", "lib", "ConfigOptions.json"),
                os.path.join(options.build_dir)
            ],
            "cwd": options.source_dir
        },
        {
            "name": "Generate Config Info (docs)",
            "group": "Codegen",
            "script": os.path.join(options.source_dir, "utils", "codegen", "generate-config-info.py"),
            "folder": "Generate Config Info",
            "args": [
                os.path.join(options.source_dir, "src", "lib", "ConfigOptions.json"),
                os.path.join(options.source_dir, "docs", "config-headers")
            ],
            "cwd": options.source_dir
        },
        {
            "name": "Generate YAML Schema",
            "group": "Codegen",
            "script": os.path.join(options.source_dir, "utils", "codegen", "generate-yaml-schema.py"),
            "args": [],
            "cwd": options.source_dir
        },
        {
            "name": "Format",
            "id": "format",
            "group": "Develop",
            "script": os.path.join(options.source_dir, "utils", "format", "format.py"),
            "args": [],
            "cwd": options.source_dir
        },
        {
            "name": "Render Docs",
            "group": "Documentation",
            "script": "npx",
            "folder": "Render Docs",
            "args": [
                "antora",
                "--fetch",
                "antora-playbook.yml",
                "--attribute",
                "branchesarray=HEAD",
                "--attribute",
                "tagsarray=",
                "--start-page",
                "mrdocs::index.adoc",
            ],
            "cwd": os.path.join(options.source_dir, "docs"),
            "env": {
                "MRDOCS_ROOT": options.install_dir,
            },
        },
        {
            "name": "Render Docs (No Reference)",
            "group": "Documentation",
            "script": "npx",
            "folder": "Render Docs",
            "args": [
                "antora",
                "--fetch",
                "antora-playbook.yml",
                "--attribute",
                "branchesarray=HEAD",
                "--attribute",
                "tagsarray=",
                "--start-page",
                "mrdocs::index.adoc",
            ],
            "cwd": os.path.join(options.source_dir, "docs"),
            "env": {
                "ANTORA_SKIP_CPP_REFERENCE": "1",
                "MRDOCS_ROOT": options.install_dir,
            },
        },
        {
            "name": "Build Docs UI Bundle",
            "group": "Documentation",
            "script": "npx",
            "folder": "Render Docs",
            "args": ["gulp", "bundle"],
            "cwd": os.path.join(options.source_dir, "docs", "ui"),
        },
        {
            "name": "Test Getting-Started Examples",
            "group": "Documentation",
            "script": os.path.join(options.source_dir, "utils", "docs", "test_getting_started.sh"),
            "folder": "Render Docs",
            "args": [],
            "cwd": options.source_dir,
            "env": {
                "MRDOCS_BIN": os.path.join(options.install_dir, "bin", "mrdocs"),
            },
        },
    ])

    # Boost documentation targets (dynamic scan)
    num_cores = os.cpu_count() or 1
    if options.boost_src_dir and os.path.exists(options.boost_src_dir):
        boost_libs = os.path.join(options.boost_src_dir, "libs")
        if os.path.exists(boost_libs):
            for lib in os.listdir(boost_libs):
                mrdocs_config = os.path.join(boost_libs, lib, "doc", "mrdocs.yml")
                if os.path.exists(mrdocs_config):
                    configs.append({
                        "name": f"Boost.{lib.title()} Documentation",
                        "group": "Documentation",
                        "target": "mrdocs",
                        "folder": "Boost Documentation",
                        "args": [
                            "../CMakeLists.txt",
                            f"--config={mrdocs_config}",
                            f"--output={os.path.join(options.boost_src_dir, 'libs', lib, 'doc', 'modules', 'reference', 'pages')}",
                            "--generator=adoc",
                            f"--addons={os.path.join(options.source_dir, 'data', 'mrdocs', 'addons')}",
                            f"--libc-includes={os.path.join(options.source_dir, 'data', 'mrdocs', 'headers', 'libc-stubs')}",
                            "--tagfile=reference.tag.xml",
                            "--multipage=true",
                            f"--concurrency={num_cores}",
                            "--log-level=debug",
                        ],
                    })

    # DOM schema regeneration. The RELAX NG schema is reflected from MrDocs's
    # own types by the `schema` generator and committed at
    # schemas/generators/mrdocs.rng, so there is no trang conversion step.
    configs.append({
        "name": "Generate DOM Schemas",
        "group": "Codegen",
        "script": os.path.join(options.source_dir, "utils", "codegen", "generate-schema.sh"),
        "args": [],
        "cwd": options.source_dir,
        "env": {
            "MRDOCS": os.path.join(options.install_dir, "bin", "mrdocs"),
            "ADDONS": os.path.join(options.source_dir, "data", "mrdocs", "addons"),
        },
    })

    return configs


def generate_run_configs(
    options: InstallOptions,
    default_options: InstallOptions,
    compiler_info: Optional[Dict[str, str]] = None,
    generate_clion: bool = True,
    generate_vscode: bool = True,
    generate_vs: bool = False,
    generate_justfile: bool = True,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
    saved_args: Optional[Dict[str, List[str]]] = None,
):
    """
    Generate run configurations for all enabled IDEs.

    This function loads the base configuration from the bootstrap manifest
    (utils/bootstrap/src/configs/run_configs.json),
    filters them based on requirements, adds dynamic configurations, and
    generates the appropriate IDE-specific config files.

    Args:
        options: Current install options.
        default_options: Default install options (for comparison).
        compiler_info: Dictionary with compiler info (CMAKE_CXX_COMPILER_ID, etc.).
        generate_clion: If True, generate CLion configurations.
        generate_vscode: If True, generate VSCode configurations.
        generate_vs: If True, generate Visual Studio configurations.
        generate_justfile: If True, generate a justfile.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
        saved_args: Saved arguments of the bootstrap entries to preserve
            (see `get_dynamic_run_configs`). The refresh-only script passes
            them; the full bootstrap run leaves this unset.
    """
    if ui is None:
        ui = get_default_ui()

    if compiler_info is None:
        compiler_info = {}

    # Load the run-configuration manifest. It ships with the bootstrap
    # package (next to this module) rather than under share/, since it is
    # dev-tooling data specific to bootstrap, not a generic runtime asset.
    defaults_path = os.path.join(os.path.dirname(__file__), "run_configs.json")
    defaults = load_json_file(defaults_path) or {}

    configs: List[Dict[str, Any]] = defaults.get("configs", [])

    if not configs:
        raise RuntimeError(f"No run configurations found in {defaults_path}; add configs to proceed.")

    # Define token replacements. run_configs.json uses the
    # `mrdocs_`-prefixed placeholders (${mrdocs_build_dir} etc.); the
    # unprefixed aliases are kept for backwards compatibility with any
    # config or test that still references $build_dir/$source_dir.
    tokens = {
        "mrdocs_build_dir": options.build_dir,
        "mrdocs_src_dir": options.source_dir,
        "mrdocs_install_dir": options.install_dir,
        "mrdocs_stdlib_includes": stdlib_includes_dir(options.source_dir, options.build_dir),
        "build_dir": options.build_dir,
        "source_dir": options.source_dir,
        "install_dir": options.install_dir,
        "docs_script_ext": "bat" if is_windows() else "sh",
        "num_cores": os.cpu_count() or 1,
    }

    # Apply token replacements
    configs = [format_values(cfg, tokens) for cfg in configs]

    # Filter configs based on requirements
    filtered = []
    for cfg in configs:
        req = cfg.get("requires", [])
        include = True
        if "build_tests" in req and not options.build_tests:
            include = False
        if "java" in req and not options.java_path:
            include = False
        if include:
            cfg.pop("requires", None)
            filtered.append(cfg)
    configs = filtered

    # A CTest entry is also a script entry for the IDE generators, which run
    # `ctest` against the active build directory. The justfile generator reads
    # the `ctest` selection itself, to name each preset's build directory.
    from .justfile import CTEST_FLAGS
    for cfg in configs:
        if "ctest" in cfg:
            cfg["script"] = "ctest"
            cfg["args"] = ["--test-dir", options.build_dir, *cfg["ctest"], *CTEST_FLAGS]
            cfg["cwd"] = options.source_dir

    # Append dynamic configs that must be computed
    # (bootstrap helpers, boost docs, schema generation)
    dynamic_configs = get_dynamic_run_configs(
        options=options,
        default_options=default_options,
        java_path=options.java_path,
        saved_args=saved_args,
    )
    configs.extend(dynamic_configs)

    # Every program path is what CMake reports for the entry's target.
    resolve_target_programs(
        configs, options.build_dir, getattr(options, "build_type", None),
        dry_run=dry_run)

    # Which IDEs/tools to generate for is a bootstrap option, not part of the
    # run-configuration data, so it comes from the generate_* flags (which are
    # driven by InstallOptions), not from the manifest.

    # Presets currently known to the project. Generators use this to prune
    # entries left over from presets that no longer exist.
    all_presets = load_preset_names(options.source_dir)

    if generate_clion:
        from .clion import generate_clion_run_configs
        ui.info("Generating CLion run configurations...")
        run_config_dir = options.jetbrains_run_config_dir or os.path.join(options.source_dir, ".run")
        generate_clion_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            run_config_dir=run_config_dir,
            all_presets=all_presets,
            dry_run=dry_run,
            ui=ui,
        )

    if generate_vscode:
        from .vscode import generate_vscode_run_configs
        ui.info("Generating Visual Studio Code run configurations...")
        generate_vscode_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            ninja_path=options.ninja_path,
            compiler_info=compiler_info,
            all_presets=all_presets,
            dry_run=dry_run,
            ui=ui,
        )

    if generate_vs:
        from .visual_studio import generate_visual_studio_run_configs
        ui.info("Generating Visual Studio run configurations...")
        generate_visual_studio_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            all_presets=all_presets,
            dry_run=dry_run,
            ui=ui,
        )

    if generate_justfile:
        from .justfile import generate_justfile_run_configs
        ui.info("Generating justfile...")
        generate_justfile_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            all_presets=all_presets,
            build_type=getattr(options, "build_type", None),
            dry_run=dry_run,
            ui=ui,
        )
