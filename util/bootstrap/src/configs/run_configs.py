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


def get_dynamic_run_configs(
    options: InstallOptions,
    default_options: InstallOptions,
    java_path: str = "",
    libxml2_root: Optional[str] = None,
) -> List[Dict[str, Any]]:
    """
    Generate dynamic run configuration data.

    Creates configuration entries for bootstrap helpers, Boost documentation
    targets, and XML/RelaxNG validation tasks.

    Args:
        options: Current install options.
        default_options: Default install options (for comparison).
        java_path: Path to Java executable.
        libxml2_root: Path to libxml2 installation root.

    Returns:
        List of run configuration dictionaries.
    """
    configs: List[Dict[str, Any]] = []

    # Bootstrap helper targets
    bootstrap_args: List[str] = []
    for field in dataclasses.fields(InstallOptions):
        value = getattr(options, field.name)
        default_value = getattr(default_options, field.name, None)
        if value is not None and (value != default_value or field.name == "build_type"):
            if field.name == "non_interactive":
                continue
            if field.type is bool:
                if value:
                    bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                else:
                    bootstrap_args.append(f"--no-{field.name.replace('_', '-')}")
            elif field.type is str:
                if value != "":
                    bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                    bootstrap_args.append(value)
            else:
                raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")

    bootstrap_refresh_config_name = options.preset or options.build_type or "debug"

    configs.extend([
        {
            "name": "MrDocs Bootstrap Help",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "args": ["--help"],
            "cwd": options.source_dir
        },
        {
            "name": f"MrDocs Bootstrap Update ({bootstrap_refresh_config_name})",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "folder": "MrDocs Bootstrap Update",
            "args": bootstrap_args,
            "cwd": options.source_dir
        },
        {
            "name": f"MrDocs Bootstrap Refresh ({bootstrap_refresh_config_name})",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "folder": "MrDocs Bootstrap Refresh",
            "args": bootstrap_args + ["--non-interactive"],
            "cwd": options.source_dir
        },
        {
            "name": "MrDocs Bootstrap Refresh All",
            "script": os.path.join(options.source_dir, "bootstrap.py"),
            "folder": "MrDocs Bootstrap Refresh",
            "args": ["--refresh-all"],
            "cwd": options.source_dir
        },
        {
            "name": f"MrDocs Generate Config Info ({bootstrap_refresh_config_name})",
            "script": os.path.join(options.source_dir, "util", "generate-config-info.py"),
            "folder": "MrDocs Generate Config Info",
            "args": [
                os.path.join(options.source_dir, "src", "lib", "ConfigOptions.json"),
                os.path.join(options.build_dir)
            ],
            "cwd": options.source_dir
        },
        {
            "name": "MrDocs Generate Config Info (docs)",
            "script": os.path.join(options.source_dir, "util", "generate-config-info.py"),
            "folder": "MrDocs Generate Config Info",
            "args": [
                os.path.join(options.source_dir, "src", "lib", "ConfigOptions.json"),
                os.path.join(options.source_dir, "docs", "config-headers")
            ],
            "cwd": options.source_dir
        },
        {
            "name": "MrDocs Generate YAML Schema",
            "script": os.path.join(options.source_dir, "util", "generate-yaml-schema.py"),
            "args": [],
            "cwd": options.source_dir
        },
        {
            "name": "MrDocs Reformat Source Files",
            "script": os.path.join(options.source_dir, "util", "reformat.py"),
            "args": [],
            "cwd": options.source_dir
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
                        "target": "mrdocs",
                        "folder": "Boost Documentation",
                        "program": os.path.join(options.build_dir, "mrdocs"),
                        "args": [
                            "../CMakeLists.txt",
                            f"--config={mrdocs_config}",
                            f"--output={os.path.join(options.boost_src_dir, 'libs', lib, 'doc', 'modules', 'reference', 'pages')}",
                            "--generator=adoc",
                            f"--addons={os.path.join(options.source_dir, 'share', 'mrdocs', 'addons')}",
                            f"--libc-includes={os.path.join(options.source_dir, 'share', 'mrdocs', 'headers', 'libc-stubs')}",
                            "--tagfile=reference.tag.xml",
                            "--multipage=true",
                            f"--concurrency={num_cores}",
                            "--log-level=debug",
                        ],
                    })

    # XML / RelaxNG tasks requiring Java and libxml2
    if java_path:
        configs.append({
            "name": "MrDocs Generate RelaxNG Schema",
            "script": java_path,
            "args": [
                "-jar",
                os.path.join(options.source_dir, "util", "trang.jar"),
                os.path.join(options.source_dir, "mrdocs.rnc"),
                os.path.join(options.build_dir, "mrdocs.rng"),
            ],
            "cwd": options.source_dir,
        })

        if libxml2_root:
            libxml2_xmllint_executable = os.path.join(libxml2_root, "bin", "xmllint")
            xml_sources_dir = os.path.join(options.source_dir, "test-files", "golden-tests")

            if is_windows():
                xml_sources = []
                for root, _, files in os.walk(xml_sources_dir):
                    for file in files:
                        if file.endswith(".xml") and not file.endswith(".bad.xml"):
                            xml_sources.append(os.path.join(root, file))
                configs.append({
                    "name": "MrDocs XML Lint with RelaxNG Schema",
                    "script": libxml2_xmllint_executable,
                    "args": [
                        "--dropdtd",
                        "--noout",
                        "--relaxng",
                        os.path.join(options.build_dir, "mrdocs.rng"),
                        *xml_sources,
                    ],
                    "cwd": options.source_dir,
                })
            else:
                configs.append({
                    "name": "MrDocs XML Lint with RelaxNG Schema",
                    "script": "find",
                    "args": [
                        xml_sources_dir,
                        "-type", "f",
                        "-name", "*.xml",
                        "!", "-name", "*.bad.xml",
                        "-exec",
                        libxml2_xmllint_executable,
                        "--dropdtd",
                        "--noout",
                        "--relaxng",
                        os.path.join(options.build_dir, "mrdocs.rng"),
                        "{}",
                        "+",
                    ],
                    "cwd": options.source_dir,
                })

    return configs


def generate_run_configs(
    options: InstallOptions,
    default_options: InstallOptions,
    package_roots: Optional[Dict[str, str]] = None,
    compiler_info: Optional[Dict[str, str]] = None,
    generate_clion: bool = True,
    generate_vscode: bool = True,
    generate_vs: bool = False,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Generate run configurations for all enabled IDEs.

    This function loads the base configuration from share/run_configs.json,
    filters them based on requirements, adds dynamic configurations, and
    generates the appropriate IDE-specific config files.

    Args:
        options: Current install options.
        default_options: Default install options (for comparison).
        package_roots: Dictionary mapping package names to their root paths.
        compiler_info: Dictionary with compiler info (CMAKE_CXX_COMPILER_ID, etc.).
        generate_clion: If True, generate CLion configurations.
        generate_vscode: If True, generate VSCode configurations.
        generate_vs: If True, generate Visual Studio configurations.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    if package_roots is None:
        package_roots = {}

    if compiler_info is None:
        compiler_info = {}

    if dry_run:
        ui.info("dry-run: skipping IDE run configuration generation")
        return

    # Variable expansion pattern
    var_pattern = re.compile(r"\$(\w+)|\${([^}]+)}")

    def expand_with(s: str, mapping: Dict[str, Any]) -> str:
        def repl(m):
            key = m.group(1) or m.group(2)
            return str(mapping.get(key, m.group(0)))
        return var_pattern.sub(repl, s)

    def format_values(obj, tokens):
        if isinstance(obj, str):
            return expand_with(obj, tokens)
        if isinstance(obj, list):
            return [format_values(x, tokens) for x in obj]
        if isinstance(obj, dict):
            return {k: format_values(v, tokens) for k, v in obj.items()}
        return obj

    # Load defaults from share/run_configs.json
    defaults_path = os.path.join(options.source_dir, "share", "run_configs.json")
    defaults = load_json_file(defaults_path) or {}

    configs: List[Dict[str, Any]] = defaults.get("configs", [])

    if not configs:
        raise RuntimeError("No run configurations found in share/run_configs.json; add configs to proceed.")

    # Define token replacements
    tokens = {
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

    # Find libxml2 root from package_roots
    libxml2_root = None
    for key, path in package_roots.items():
        if "libxml2" in key.lower():
            libxml2_root = path
            break

    # Append dynamic configs that must be computed
    # (bootstrap helpers, boost docs, schema lint)
    dynamic_configs = get_dynamic_run_configs(
        options=options,
        default_options=default_options,
        java_path=options.java_path,
        libxml2_root=libxml2_root,
    )
    configs.extend(dynamic_configs)

    # Determine which IDEs to target based on defaults
    target_vscode = bool(defaults.get("vscode", True))
    target_clion = bool(defaults.get("clion", True))
    target_vs = bool(defaults.get("vs", True))

    if target_clion and generate_clion:
        from .clion import generate_clion_run_configs
        ui.info("Generating CLion run configurations...")
        run_config_dir = options.jetbrains_run_config_dir or os.path.join(options.source_dir, ".run")
        generate_clion_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            run_config_dir=run_config_dir,
            dry_run=dry_run,
            ui=ui,
        )

    if target_vscode and generate_vscode:
        from .vscode import generate_vscode_run_configs
        ui.info("Generating Visual Studio Code run configurations...")
        generate_vscode_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            ninja_path=options.ninja_path,
            compiler_info=compiler_info,
            dry_run=dry_run,
            ui=ui,
        )

    if target_vs and generate_vs:
        from .visual_studio import generate_visual_studio_run_configs
        ui.info("Generating Visual Studio run configurations...")
        generate_visual_studio_run_configs(
            configs=configs,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            preset=options.preset,
            dry_run=dry_run,
            ui=ui,
        )
