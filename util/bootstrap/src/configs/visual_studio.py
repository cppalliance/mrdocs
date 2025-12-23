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
Visual Studio configuration generation.

Generates launch.vs.json and tasks.vs.json for Visual Studio.
"""

import json
import os
import subprocess
from typing import Optional, List, Dict, Any

from ..core.filesystem import ensure_dir, write_text, load_json_file
from ..core.ui import TextUI, get_default_ui


def generate_visual_studio_run_configs(
    configs: List[Dict[str, Any]],
    source_dir: str,
    build_dir: str,
    preset: str,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Generate Visual Studio launch.vs.json and tasks.vs.json files.

    Args:
        configs: List of run configuration dictionaries.
        source_dir: MrDocs source directory.
        build_dir: Build directory.
        preset: Preset name.
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.

    References:
        https://learn.microsoft.com/en-us/visualstudio/ide/customize-build-and-debug-tasks-in-visual-studio
        https://learn.microsoft.com/en-us/cpp/build/launch-vs-schema-reference-cpp
        https://learn.microsoft.com/en-us/cpp/build/tasks-vs-json-schema-reference-cpp
    """
    if ui is None:
        ui = get_default_ui()

    vs_dir = os.path.join(source_dir, ".vs")

    if dry_run:
        ui.info(f"dry-run: would generate Visual Studio configs in {vs_dir}")
        return

    ensure_dir(vs_dir, dry_run=False, ui=ui)

    launch_path = os.path.join(vs_dir, "launch.vs.json")
    tasks_path = os.path.join(vs_dir, "tasks.vs.json")

    # Load existing configs if present
    launch_data = load_json_file(launch_path) or {"version": "0.2.1", "defaults": {}, "configurations": []}
    tasks_data = load_json_file(tasks_path) or {"version": "0.2.1", "tasks": []}

    # Build a dict for quick lookup by name
    vs_configs_by_name = {cfg.get("name"): cfg for cfg in launch_data.get("configurations", [])}
    vs_tasks_by_name = {task.get("taskLabel"): task for task in tasks_data.get("tasks", [])}

    def vs_config_type(config):
        """Determine Visual Studio config type based on script or target."""
        if "script" in config:
            if config["script"].endswith(".py"):
                return "python"
            elif config["script"].endswith(".js"):
                return "nodejs"
            else:
                return "shell"
        elif "target" in config:
            return "default"
        return None

    def rel_to_mrdocs_dir(script_path):
        """Convert absolute path to relative path from source_dir."""
        is_subdir_of_source_dir = script_path.replace('\\', '/').rstrip('/').startswith(
            source_dir.replace('\\', '/').rstrip('/'))
        if is_subdir_of_source_dir:
            return os.path.relpath(script_path, source_dir)
        return script_path

    def vs_config_project(config):
        """Determine project file for the configuration."""
        if "target" in config:
            return "CMakeLists.txt"
        elif "script" in config:
            return rel_to_mrdocs_dir(config["script"])
        return None

    def vs_config_project_target(config):
        """Determine project target for the configuration."""
        if "target" in config:
            return config["target"] + ".exe"
        return ""

    for config in configs:
        is_python_script = 'script' in config and config['script'].endswith('.py')
        is_config = 'target' in config or is_python_script

        if is_config:
            new_cfg = {
                "name": config["name"],
                "type": vs_config_type(config),
                "project": vs_config_project(config),
                "projectTarget": vs_config_project_target(config)
            }

            if "cwd" in config:
                new_cfg["cwd"] = config["cwd"]
            if "env" in config:
                new_cfg["env"] = config["env"]

            if 'target' in config:
                if "args" in config:
                    new_cfg["args"] = config["args"]

            if 'script' in config:
                new_cfg["interpreter"] = "(default)"
                new_cfg["interpreterArguments"] = ''
                if "args" in config and isinstance(config["args"], list):
                    new_cfg["scriptArguments"] = subprocess.list2cmdline(config["args"])
                else:
                    new_cfg["scriptArguments"] = ""
                new_cfg["nativeDebug"] = False
                new_cfg["webBrowserUrl"] = ""

            # Replace or add
            vs_configs_by_name[new_cfg["name"]] = new_cfg

        else:
            # This is a task configuration
            new_task = {
                "taskLabel": config["name"],
                # appliesTo script meaning we'll see the tasks as an option
                # when right-clicking on the script in Visual Studio
                "appliesTo": vs_config_project(config),
                "type": "launch",
                "command": config.get("script", ""),
                "args": config.get("args", []),
            }

            if 'env' in config:
                new_task["env"] = config["env"]

            if 'cwd' in config:
                new_task["workingDirectory"] = config["cwd"]

            if new_task["command"].endswith(".js"):
                new_task["args"] = [new_task["command"]] + new_task["args"]
                new_task["command"] = "node"
            elif new_task["command"] == "npm" and "workingDirectory" in new_task:
                new_task["appliesTo"] = os.path.join(new_task["workingDirectory"], "package.json")
                new_task["appliesTo"] = rel_to_mrdocs_dir(new_task["appliesTo"])
            elif new_task["taskLabel"] == "MrDocs Generate RelaxNG Schema":
                new_task["appliesTo"] = "mrdocs.rnc"
            elif new_task["taskLabel"] == "MrDocs XML Lint with RelaxNG Schema":
                new_task["appliesTo"] = "mrdocs.rng"

            vs_tasks_by_name[new_task["taskLabel"]] = new_task

    # Write back all configs
    launch_data["configurations"] = list(vs_configs_by_name.values())
    write_text(launch_path, json.dumps(launch_data, indent=4), dry_run=False, ui=ui)

    tasks_data["tasks"] = list(vs_tasks_by_name.values())
    write_text(tasks_path, json.dumps(tasks_data, indent=4), dry_run=False, ui=ui)
