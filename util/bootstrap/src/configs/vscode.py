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
VSCode configuration generation.

Generates launch.json and tasks.json for Visual Studio Code.
"""

import json
import os
import shutil
from typing import Optional, List, Dict, Any

from ..core.filesystem import ensure_dir, write_text, load_json_file
from ..core.ui import TextUI, get_default_ui


def replace_with_placeholders(config: Dict[str, Any], source_dir: str) -> None:
    """Replace source_dir paths with ${workspaceFolder} in a config dict (in-place)."""
    for key, value in config.items():
        if isinstance(value, str):
            config[key] = value.replace(source_dir, "${workspaceFolder}")
        elif isinstance(value, list):
            for i in range(len(value)):
                if isinstance(value[i], str):
                    value[i] = value[i].replace(source_dir, "${workspaceFolder}")
        elif isinstance(value, dict):
            for sub_key, sub_value in value.items():
                if isinstance(sub_value, str):
                    value[sub_key] = sub_value.replace(source_dir, "${workspaceFolder}")


def to_task_args(cfg: Dict[str, Any]) -> List:
    """Extract args list from a config dict, returning a copy or empty list."""
    if 'args' in cfg and isinstance(cfg['args'], list):
        return cfg['args'].copy()
    return []


def generate_vscode_run_configs(
    configs: List[Dict[str, Any]],
    source_dir: str,
    build_dir: str,
    preset: str,
    ninja_path: Optional[str] = None,
    compiler_info: Optional[Dict[str, str]] = None,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Generate VSCode launch.json and tasks.json files.

    Args:
        configs: List of run configuration dictionaries.
        source_dir: MrDocs source directory.
        build_dir: Build directory.
        preset: Preset name.
        ninja_path: Path to ninja executable (optional).
        compiler_info: Dictionary with CMAKE_CXX_COMPILER_ID, CMAKE_CXX_COMPILER, etc.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    if compiler_info is None:
        compiler_info = {}

    vscode_dir = os.path.join(source_dir, ".vscode")

    ensure_dir(vscode_dir, dry_run=dry_run, ui=ui)

    launch_path = os.path.join(vscode_dir, "launch.json")
    tasks_path = os.path.join(vscode_dir, "tasks.json")

    # Load existing configs if present (empty defaults for dry-run when files don't exist)
    launch_data = load_json_file(launch_path) or {"version": "0.2.0", "configurations": []}
    tasks_data = load_json_file(tasks_path) or {"version": "2.0.0", "tasks": []}

    # Build a dict for quick lookup by name
    vs_configs_by_name = {cfg.get("name"): cfg for cfg in launch_data.get("configurations", [])}
    vs_tasks_by_name = {task.get("label"): task for task in tasks_data.get("tasks", [])}

    bootstrap_refresh_config_name = preset or "debug"

    for config in configs:
        is_python_script = 'script' in config and config['script'].endswith('.py')
        is_js_script = 'script' in config and config['script'].endswith('.js')
        is_config = 'target' in config or is_python_script or is_js_script

        if is_config:
            new_cfg = {
                "name": config["name"],
                "type": None,
                "request": "launch",
                "program": config.get("script", "") or config.get("target", ""),
                "args": config.get("args", []).copy() if isinstance(config.get("args"), list) else [],
                "cwd": config.get('cwd', build_dir)
            }

            if 'target' in config:
                new_cfg["name"] += f" ({bootstrap_refresh_config_name})"
                new_cfg["type"] = "cppdbg"
                if 'program' in config:
                    new_cfg["program"] = config["program"]
                else:
                    new_cfg["program"] = os.path.join(build_dir, config["target"])
                new_cfg["environment"] = []
                new_cfg["stopAtEntry"] = False
                new_cfg["externalConsole"] = False
                new_cfg["preLaunchTask"] = f"CMake Build {config['target']} ({bootstrap_refresh_config_name})"

                # Determine MIMode based on compiler
                compiler_id = compiler_info.get("CMAKE_CXX_COMPILER_ID", "").lower()
                if compiler_id in ("clang", "appleclang"):
                    lldb_path = shutil.which("lldb")
                    if not lldb_path:
                        clang_path = compiler_info.get("CMAKE_CXX_COMPILER", "")
                        if clang_path and os.path.exists(clang_path):
                            candidate = os.path.join(os.path.dirname(clang_path), "lldb")
                            if os.path.exists(candidate):
                                lldb_path = candidate
                    if lldb_path:
                        new_cfg["MIMode"] = "lldb"
                        new_cfg["miDebuggerPath"] = lldb_path
                else:
                    gdb_path = shutil.which("gdb")
                    if not gdb_path:
                        compiler_path = compiler_info.get("CMAKE_CXX_COMPILER", "")
                        if compiler_path and os.path.exists(compiler_path):
                            candidate = os.path.join(os.path.dirname(compiler_path), "gdb")
                            if os.path.exists(candidate):
                                gdb_path = candidate
                    if gdb_path:
                        new_cfg["MIMode"] = "gdb"
                        new_cfg["miDebuggerPath"] = gdb_path

            if 'script' in config:
                new_cfg["program"] = config["script"]
                if config["script"].endswith(".py"):
                    new_cfg["type"] = "debugpy"
                    new_cfg["console"] = "integratedTerminal"
                    new_cfg["stopOnEntry"] = False
                    new_cfg["justMyCode"] = True
                    new_cfg["env"] = {}
                elif config["script"].endswith(".js"):
                    new_cfg["type"] = "node"
                    new_cfg["console"] = "integratedTerminal"
                    new_cfg["internalConsoleOptions"] = "neverOpen"
                    new_cfg["skipFiles"] = ["<node_internals>/**"]
                    new_cfg["sourceMaps"] = True
                    new_cfg["env"] = {}
                    for key, value in config.get("env", {}).items():
                        new_cfg["env"][key] = value
                else:
                    raise ValueError(
                        f"Unsupported script type for configuration '{config['name']}': {config['script']}. "
                        "Only Python (.py) and JavaScript (.js) scripts are supported."
                    )

            replace_with_placeholders(new_cfg, source_dir)
            vs_configs_by_name[new_cfg["name"]] = new_cfg

        else:
            # This is a script configuration, we will create a task for it
            new_task = {
                "label": config["name"],
                "type": "shell",
                "command": config.get("script", ""),
                "args": to_task_args(config),
                "options": {},
                "problemMatcher": [],
            }
            if 'cwd' in config and config["cwd"] != source_dir:
                new_task["options"]["cwd"] = config["cwd"]

            replace_with_placeholders(new_task, source_dir)
            vs_tasks_by_name[new_task["label"]] = new_task

    # Create tasks for the cmake config and build steps
    cmake_config_args = ["-S", "${workspaceFolder}"]
    if preset:
        cmake_config_args.extend(["--preset", preset])
    else:
        cmake_config_args.extend(["-B", build_dir])
        if ninja_path:
            cmake_config_args.extend(["-G", "Ninja"])

    cmake_config_task = {
        "label": f"CMake Configure ({bootstrap_refresh_config_name})",
        "type": "shell",
        "command": "cmake",
        "args": cmake_config_args,
        "options": {"cwd": "${workspaceFolder}"}
    }
    replace_with_placeholders(cmake_config_task, source_dir)
    vs_tasks_by_name[cmake_config_task["label"]] = cmake_config_task

    # Create build tasks for unique targets
    unique_targets = set()
    for config in configs:
        if 'target' in config:
            unique_targets.add(config['target'])

    for target in unique_targets:
        build_args = ["--build", build_dir, "--target", target]
        cmake_build_task = {
            "label": f"CMake Build {target} ({bootstrap_refresh_config_name})",
            "type": "shell",
            "command": "cmake",
            "args": build_args,
            "options": {"cwd": "${workspaceFolder}"},
            "dependsOn": f"CMake Configure ({bootstrap_refresh_config_name})",
            "dependsOrder": "sequence",
            "group": "build"
        }
        replace_with_placeholders(cmake_build_task, source_dir)
        vs_tasks_by_name[cmake_build_task["label"]] = cmake_build_task

    # Write back all configs
    launch_data["configurations"] = list(vs_configs_by_name.values())
    write_text(launch_path, json.dumps(launch_data, indent=4) + "\n", dry_run=dry_run, ui=ui)

    tasks_data["tasks"] = list(vs_tasks_by_name.values())
    write_text(tasks_path, json.dumps(tasks_data, indent=4) + "\n", dry_run=dry_run, ui=ui)
