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
CLion run configuration generation.

Generates XML run configuration files for JetBrains CLion IDE.
"""

import os
import shlex
import xml.etree.ElementTree as ET
from typing import Optional, List, Dict, Any

from ..core.filesystem import ensure_dir, write_text
from ..core.ui import TextUI, get_default_ui


def _env_prefix(config: Dict[str, Any]) -> str:
    """Return a shell prefix that sets each `env` entry inline for the
    command that follows, e.g. `KEY='value' OTHER='x' `. Empty when the
    config has no env entries. CLion's Shell run plugin does forward
    `<envs>` to the child process via GeneralCommandLine, but in
    `EXECUTE_IN_TERMINAL=false` mode the npx-shell-node chain we use
    doesn't always inherit them reliably. Embedding the assignments in
    SCRIPT_TEXT makes them load-bearing for execution; the `<envs>` block
    is still emitted so the IDE shows the same values in its UI."""
    env = config.get("env") or {}
    if not env:
        return ""
    return " ".join(f"{k}={shlex.quote(v)}" for k, v in env.items()) + " "


def generate_clion_run_configs(
    configs: List[Dict[str, Any]],
    source_dir: str,
    build_dir: str,
    preset: str,
    run_config_dir: Optional[str] = None,
    all_presets: Optional[List[str]] = None,
    dry_run: bool = False,
    ui: Optional[TextUI] = None,
):
    """
    Generate CLion run configuration XML files.

    Args:
        configs: List of run configuration dictionaries.
        source_dir: MrDocs source directory.
        build_dir: Build directory.
        preset: Preset name.
        run_config_dir: Directory to write run configs. Defaults to source_dir/.run
        dry_run: If True, only print what would be done.
        ui: TextUI instance for output.
    """
    if ui is None:
        ui = get_default_ui()

    if run_config_dir is None:
        run_config_dir = os.path.join(source_dir, ".run")

    ensure_dir(run_config_dir, dry_run=dry_run, ui=ui)

    for config in configs:
        # Interface/aggregate configs (depends-only, no target or script) are
        # a justfile concept; CLion has nothing to emit for them.
        if 'target' not in config and 'script' not in config:
            continue
        config_name = config["name"]
        run_config_path = os.path.join(run_config_dir, f"{config_name}.run.xml")
        root = ET.Element("component", name="ProjectRunConfigurationManager")
        # Ownership marker so stale generated files can be pruned later
        # without ever deleting hand-authored run configurations.
        root.append(ET.Comment(" mrdocs:generated "))

        if 'target' in config:
            # CMake target configuration
            attrib = {
                "default": "false",
                "name": config["name"],
                "type": "CMakeRunConfiguration",
                "factoryName": "Application",
                "PROGRAM_PARAMS": ' '.join(shlex.quote(arg) for arg in config.get("args", [])),
                "REDIRECT_INPUT": "false",
                "ELEVATE": "false",
                "USE_EXTERNAL_CONSOLE": "false",
                "EMULATE_TERMINAL": "false",
                "PASS_PARENT_ENVS_2": "true",
                "PROJECT_NAME": "MrDocs",
                "TARGET_NAME": config["target"],
                "CONFIG_NAME": preset or "debug",
                "RUN_TARGET_PROJECT_NAME": "MrDocs",
                "RUN_TARGET_NAME": config["target"]
            }
            if 'folder' in config:
                attrib["folderName"] = config["folder"]
            clion_config = ET.SubElement(root, "configuration", attrib)
            if 'env' in config:
                envs = ET.SubElement(clion_config, "envs")
                for key, value in config['env'].items():
                    ET.SubElement(envs, "env", name=key, value=value)
            method = ET.SubElement(clion_config, "method", v="2")
            ET.SubElement(method, "option",
                          name="com.jetbrains.cidr.execution.CidrBuildBeforeRunTaskProvider$BuildBeforeRunTask",
                          enabled="true")

        elif 'script' in config:
            if config["script"].endswith(".py"):
                # Python script configuration
                attrib = {
                    "default": "false",
                    "name": config["name"],
                    "type": "PythonConfigurationType",
                    "factoryName": "Python",
                    "nameIsGenerated": "false"
                }
                if 'folder' in config:
                    attrib["folderName"] = config["folder"]
                clion_config = ET.SubElement(root, "configuration", attrib)
                ET.SubElement(clion_config, "module", name="mrdocs")
                ET.SubElement(clion_config, "option", name="ENV_FILES", value="")
                ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                ET.SubElement(clion_config, "option", name="PARENT_ENVS", value="true")
                envs = ET.SubElement(clion_config, "envs")
                ET.SubElement(envs, "env", name="PYTHONUNBUFFERED", value="1")
                ET.SubElement(clion_config, "option", name="SDK_HOME", value="")
                if 'cwd' in config and config["cwd"] != source_dir:
                    ET.SubElement(clion_config, "option", name="WORKING_DIRECTORY", value=config["cwd"])
                else:
                    ET.SubElement(clion_config, "option", name="WORKING_DIRECTORY", value="$PROJECT_DIR$")
                ET.SubElement(clion_config, "option", name="IS_MODULE_SDK", value="true")
                ET.SubElement(clion_config, "option", name="ADD_CONTENT_ROOTS", value="true")
                ET.SubElement(clion_config, "option", name="ADD_SOURCE_ROOTS", value="true")
                ET.SubElement(clion_config, "option", name="SCRIPT_NAME", value=config["script"])
                ET.SubElement(clion_config, "option", name="PARAMETERS",
                              value=' '.join(shlex.quote(arg) for arg in config.get("args", [])))
                ET.SubElement(clion_config, "option", name="SHOW_COMMAND_LINE", value="false")
                ET.SubElement(clion_config, "option", name="EMULATE_TERMINAL", value="false")
                ET.SubElement(clion_config, "option", name="MODULE_MODE", value="false")
                ET.SubElement(clion_config, "option", name="REDIRECT_INPUT", value="false")
                ET.SubElement(clion_config, "option", name="INPUT_FILE", value="")
                ET.SubElement(clion_config, "method", v="2")

            elif config["script"].endswith(".sh"):
                # Shell script configuration
                attrib = {
                    "default": "false",
                    "name": config["name"],
                    "type": "ShConfigurationType"
                }
                if 'folder' in config:
                    attrib["folderName"] = config["folder"]
                clion_config = ET.SubElement(root, "configuration", attrib)
                ET.SubElement(clion_config, "option", name="SCRIPT_TEXT",
                              value=f"{_env_prefix(config)}bash {shlex.quote(config['script'])}")
                ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_PATH", value="true")
                ET.SubElement(clion_config, "option", name="SCRIPT_PATH", value=config["script"])
                ET.SubElement(clion_config, "option", name="SCRIPT_OPTIONS", value="")
                ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_WORKING_DIRECTORY", value="true")
                if 'cwd' in config and config["cwd"] != source_dir:
                    ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value=config["cwd"])
                else:
                    ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value="$PROJECT_DIR$")
                ET.SubElement(clion_config, "option", name="INDEPENDENT_INTERPRETER_PATH", value="true")
                ET.SubElement(clion_config, "option", name="INTERPRETER_PATH", value="")
                ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                # Run inside the Run tool window instead of spawning a
                # new external terminal each invocation. The Run window
                # is reused and gives the same scrollback/search the
                # IDE provides for other configurations.
                ET.SubElement(clion_config, "option", name="EXECUTE_IN_TERMINAL", value="false")
                ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="false")
                # Forward `env` from the config entry. The CLion Shell
                # plugin uses `EnvironmentVariablesData`, which expects
                # `<envs pass-parent-envs="true"><env name=.. value=../></envs>`.
                # `pass-parent-envs` defaults to true but writing it
                # explicitly matches CLion's own canonical serialization,
                # so a hand-edited config and a generated one stay
                # byte-identical and the env field is guaranteed to
                # populate in the run-config dialog.
                envs = ET.SubElement(clion_config, "envs", attrib={"pass-parent-envs": "true"})
                for key, value in config.get("env", {}).items():
                    ET.SubElement(envs, "env", name=key, value=value)
                ET.SubElement(clion_config, "method", v="2")

            elif config["script"].endswith(".js"):
                # Node.js script configuration
                attrib = {
                    "default": "false",
                    "name": config["name"],
                    "type": "NodeJSConfigurationType",
                    "path-to-js-file": config["script"],
                    "working-dir": config.get("cwd", "$PROJECT_DIR$")
                }
                if 'folder' in config:
                    attrib["folderName"] = config["folder"]
                clion_config = ET.SubElement(root, "configuration", attrib)
                envs = ET.SubElement(clion_config, "envs")
                if 'env' in config:
                    for key, value in config['env'].items():
                        ET.SubElement(envs, "env", name=key, value=value)
                ET.SubElement(clion_config, "method", v="2")

            elif config["script"] == "npm":
                # npm script configuration
                attrib = {
                    "default": "false",
                    "name": config["name"],
                    "type": "js.build_tools.npm"
                }
                if 'folder' in config:
                    attrib["folderName"] = config["folder"]
                clion_config = ET.SubElement(root, "configuration", attrib)
                ET.SubElement(clion_config, "package-json", value=os.path.join(config["cwd"], "package.json"))
                ET.SubElement(clion_config, "command", value=config["args"][0] if config.get("args") else "ci")
                ET.SubElement(clion_config, "node-interpreter", value="project")
                envs = ET.SubElement(clion_config, "envs")
                if 'env' in config:
                    for key, value in config['env'].items():
                        ET.SubElement(envs, "env", name=key, value=value)
                ET.SubElement(clion_config, "method", v="2")

            else:
                # Generic shell configuration fallback
                attrib = {
                    "default": "false",
                    "name": config["name"],
                    "type": "ShConfigurationType"
                }
                if 'folder' in config:
                    attrib["folderName"] = config["folder"]
                clion_config = ET.SubElement(root, "configuration", attrib)
                args = config.get("args") or []
                ET.SubElement(clion_config, "option", name="SCRIPT_TEXT",
                              value=f"{_env_prefix(config)}{shlex.quote(config['script'])} {' '.join(shlex.quote(arg) for arg in args)}")
                ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_PATH", value="true")
                ET.SubElement(clion_config, "option", name="SCRIPT_PATH", value=config["script"])
                ET.SubElement(clion_config, "option", name="SCRIPT_OPTIONS", value="")
                ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_WORKING_DIRECTORY", value="true")
                if 'cwd' in config and config["cwd"] != source_dir:
                    ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value=config["cwd"])
                else:
                    ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value="$PROJECT_DIR$")
                ET.SubElement(clion_config, "option", name="INDEPENDENT_INTERPRETER_PATH", value="true")
                ET.SubElement(clion_config, "option", name="INTERPRETER_PATH", value="")
                ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                # Same as the .sh branch above: keep output in the Run
                # tool window rather than opening a fresh terminal tab
                # for every invocation.
                ET.SubElement(clion_config, "option", name="EXECUTE_IN_TERMINAL", value="false")
                ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="false")
                # Forward `env` from the config entry. The CLion Shell
                # plugin uses `EnvironmentVariablesData`, which expects
                # `<envs pass-parent-envs="true"><env name=.. value=../></envs>`.
                # `pass-parent-envs` defaults to true but writing it
                # explicitly matches CLion's own canonical serialization,
                # so a hand-edited config and a generated one stay
                # byte-identical and the env field is guaranteed to
                # populate in the run-config dialog.
                envs = ET.SubElement(clion_config, "envs", attrib={"pass-parent-envs": "true"})
                for key, value in config.get("env", {}).items():
                    ET.SubElement(envs, "env", name=key, value=value)
                ET.SubElement(clion_config, "method", v="2")

        tree = ET.ElementTree(root)
        if dry_run:
            xml_content = ET.tostring(root, encoding="unicode")
            write_text(run_config_path, xml_content + "\n", dry_run=True, ui=ui)
        else:
            tree.write(run_config_path, encoding="utf-8", xml_declaration=False)

    # Prune generator-owned run-config files for configs that no longer
    # exist. Only files carrying the ownership marker are removed; files a
    # user created by hand are left untouched.
    if not dry_run and os.path.isdir(run_config_dir):
        valid_files = {f"{config['name']}.run.xml" for config in configs}
        for fname in os.listdir(run_config_dir):
            if not fname.endswith(".run.xml") or fname in valid_files:
                continue
            path = os.path.join(run_config_dir, fname)
            try:
                with open(path, "r", encoding="utf-8") as f:
                    content = f.read()
            except OSError:
                continue
            if "mrdocs:generated" in content:
                os.remove(path)
                ui.info(f"Removed stale run configuration: {fname}")
