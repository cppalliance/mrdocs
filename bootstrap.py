#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

import argparse
import subprocess
import os
import sys
import shutil
from dataclasses import dataclass, field
import dataclasses
import urllib.request
import tarfile
import json
import shlex
import re
from functools import lru_cache


@lru_cache(maxsize=1)
def running_from_mrdocs_source_dir():
    """
    Checks if the current working directory is the same as the script directory.
    :return:
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    cwd = os.getcwd()
    return cwd == script_dir


@dataclass
class InstallOptions:
    """
    Stores configuration options for the MrDocs bootstrap installer.

    Note:
        The @dataclass decorator is used to automatically generate special methods for
        the class, such as __init__, __repr__, and __eq__, based on the class attributes.
        This simplifies the creation of classes that are primarily used to store data,
        reducing boilerplate code and improving readability.
        In InstallOptions, it allows easy initialization and management of
        configuration options with default values and type hints.
    """
    # Tools
    git_path: str = ''
    cmake_path: str = ''

    # MrDocs
    mrdocs_src_dir: str = field(
        default_factory=lambda: os.getcwd() if running_from_mrdocs_source_dir() else os.path.join(os.getcwd(),
                                                                                                  "mrdocs"))
    mrdocs_build_type: str = "Release"
    mrdocs_repo: str = "https://github.com/cppalliance/mrdocs"
    mrdocs_branch: str = "develop"
    mrdocs_use_user_presets: bool = True
    mrdocs_preset_name: str = "<mrdocs-build-type:lower>-<os:lower>"
    mrdocs_build_dir: str = "<mrdocs-src-dir>/build/<mrdocs-build-type:lower>-<os:lower>"
    mrdocs_build_tests: bool = True
    mrdocs_system_install: bool = field(default_factory=lambda: not running_from_mrdocs_source_dir())
    mrdocs_install_dir: str = field(
        default_factory=lambda: "<mrdocs-src-dir>/install/<mrdocs-build-type:lower>-<os:lower>" if running_from_mrdocs_source_dir() else "")
    mrdocs_run_tests: bool = True

    # Third-party dependencies
    third_party_src_dir: str = "<mrdocs-src-dir>/../third-party"

    # Duktape
    duktape_src_dir: str = "<third-party-src-dir>/duktape"
    duktape_url: str = "https://github.com/svaarala/duktape/releases/download/v2.7.0/duktape-2.7.0.tar.xz"
    duktape_build_type: str = "<mrdocs-build-type>"
    duktape_build_dir: str = "<duktape-src-dir>/build/<duktape-build-type:lower>"
    duktape_install_dir: str = "<duktape-src-dir>/install/<duktape-build-type:lower>"

    # Libxml2
    libxml2_src_dir: str = "<third-party-src-dir>/libxml2"
    # purposefully does not depend on mrdocs-build-type because we only need the executable
    libxml2_build_type: str = "Release"
    libxml2_build_dir: str = "<libxml2-src-dir>/build/<libxml2-build-type:lower>"
    libxml2_install_dir: str = "<libxml2-src-dir>/install/<libxml2-build-type:lower>"
    libxml2_repo: str = "https://github.com/GNOME/libxml2"
    libxml2_branch: str = "v2.12.6"

    # LLVM
    llvm_src_dir: str = "<third-party-src-dir>/llvm-project"
    llvm_build_type: str = "<mrdocs-build-type>"
    llvm_build_dir: str = "<llvm-src-dir>/build/<llvm-build-type:lower>"
    llvm_install_dir: str = "<llvm-src-dir>/install/<llvm-build-type:lower>"
    llvm_repo: str = "https://github.com/llvm/llvm-project.git"
    llvm_commit: str = "dd7a3d4d798e30dfe53b5bbbbcd9a23c24ea1af9"

    # Information to create run configurations
    generate_run_configs: bool = field(default_factory=lambda: running_from_mrdocs_source_dir())
    jetbrains_run_config_dir: str = "<mrdocs-src-dir>/.run"
    boost_src_dir: str = "<mrdocs-src-dir>/../boost"

    # Meta
    non_interactive: bool = False


# Constant for option descriptions
INSTALL_OPTION_DESCRIPTIONS = {
    "git_path": "Path to the git executable, if not in system PATH.",
    "cmake_path": "Path to the cmake executable, if not in system PATH.",
    "mrdocs_src_dir": "MrDocs source directory.",
    "mrdocs_repo": "URL of the MrDocs repository to clone.",
    "mrdocs_branch": "Branch or tag of the MrDocs repository to use.",
    "mrdocs_build_type": "CMake build type for MrDocs (Release, Debug, RelWithDebInfo, MilRelSize, DebWithOpt).",
    "mrdocs_use_user_presets": "Whether to use CMake User Presets for building MrDocs.",
    "mrdocs_preset_name": "Name of the CMake User Preset to use for MrDocs.",
    "mrdocs_build_dir": "Directory where MrDocs will be built.",
    "mrdocs_build_tests": "Whether to build tests for MrDocs.",
    "mrdocs_install_dir": "Directory where MrDocs will be installed.",
    "mrdocs_system_install": "Whether to install MrDocs to the system directories instead of a custom install directory.",
    "mrdocs_run_tests": "Whether to run tests after building MrDocs.",
    "third_party_src_dir": "Directory for all third-party source dependencies.",
    "duktape_src_dir": "Directory for the Duktape source code.",
    "duktape_url": "Download URL for the Duktape source archive.",
    "duktape_build_type": "CMake build type for Duktape. (Release, Debug, RelWithDebInfo, MilRelSize, DebWithOpt)",
    "duktape_build_dir": "Directory where Duktape will be built.",
    "duktape_install_dir": "Directory where Duktape will be installed.",
    "libxml2_src_dir": "Directory for the libxml2 source code.",
    "libxml2_build_type": "CMake build type for libxml2. (Release, Debug, RelWithDebInfo, MilRelSize, DebWithOpt)",
    "libxml2_build_dir": "Directory where libxml2 will be built.",
    "libxml2_install_dir": "Directory where libxml2 will be installed.",
    "libxml2_repo": "URL of the libxml2 repository to clone.",
    "libxml2_branch": "Branch or tag of libxml2 to use.",
    "llvm_src_dir": "Directory for the LLVM project source code.",
    "llvm_build_type": "CMake build type for LLVM. (Release, Debug, RelWithDebInfo, MilRelSize, DebWithOpt)",
    "llvm_build_dir": "Directory where LLVM will be built.",
    "llvm_install_dir": "Directory where LLVM will be installed.",
    "llvm_repo": "URL of the LLVM project repository to clone.",
    "llvm_commit": "Specific commit hash of LLVM to checkout.",
    "generate_run_configs": "Whether to generate run configurations for IDEs.",
    "jetbrains_run_config_dir": "Directory where JetBrains run configurations will be stored.",
    "boost_src_dir": "Directory where the source files of the Boost libraries are located.",
    "non_interactive": "Whether to use all default options without interactive prompts."
}


class MrDocsInstaller:
    """
    Handles the installation workflow for MrDocs and its third-party dependencies.
    """

    def __init__(self, cmd_line_args=None):
        """
        Initializes the installer with the given options.
        """
        self.cmd_line_args = cmd_line_args or dict()
        self.default_options = InstallOptions()
        self.options = InstallOptions()
        for field in dataclasses.fields(self.options):
            if field.type == str:
                setattr(self.options, field.name, '')
            elif field.type == bool:
                setattr(self.options, field.name, None)
            else:
                raise TypeError(f"Unsupported type {field.type} for field {field.name} in InstallOptions.")
        self.options.non_interactive = self.cmd_line_args.get("non_interactive", False)
        self.prompted_options = set()

    def prompt_string(self, prompt, default):
        """
        Prompts the user for a string input with a default value.

        :param prompt: The prompt message to display to the user.
        : param default: The default value to use if the user does not provide input.
        :return:
        """
        if self.options.non_interactive and default is not None:
            return default
        prompt = prompt.strip()
        if prompt.endswith('.'):
            prompt = prompt[:-1]
            prompt = prompt.strip()
        BLUE = "\033[94m"
        RESET = "\033[0m"
        if self.supports_ansi():
            prompt = f"{BLUE}{prompt}{RESET}"
        inp = input(f"{prompt} ({default}): ")
        result = inp.strip() or default
        return result

    def prompt_boolean(self, prompt, default=None):
        """
        Prompts the user for a boolean value (yes/no).

        :param prompt: The prompt message to display.
        :param default: The default value to return if the user does not provide input.
        :return: bool: True if the user answers yes, False otherwise.
        """
        if self.options.non_interactive and default is not None:
            return default
        prompt = prompt.strip()
        if prompt.endswith('.'):
            prompt = prompt[:-1]
            prompt = prompt.strip()
        BLUE = "\033[94m"
        RESET = "\033[0m"
        if self.supports_ansi():
            prompt = f"{BLUE}{prompt}{RESET}"
        while True:
            answer = input(f"{prompt} ({'y/n' if default is None else 'yes' if default else 'no'}): ").strip().lower()
            if answer in ('y', 'yes'):
                return True
            elif answer in ('n', 'no'):
                return False
            else:
                if default is not None:
                    return default
                else:
                    print("Please answer 'y or 'n'.")

    def prompt_option(self, name, force_prompt=False):
        """
        Prompts the user for a configuration option based on its name.

        This function will prompt the user for a specific option if
        it has not been prompted before.

        Values come from command line arguments, from the default options,
        or from user input.

        :param name: The name of the option to prompt for.
        :return: The value of the option after prompting the user.
        """
        name = name.replace("-", "_")

        # If already prompted for this one
        if name in self.prompted_options and not force_prompt:
            return getattr(self.options, name)

        # Determine the default value for the option
        default_value = getattr(self.default_options, name, None)
        if default_value is None:
            raise ValueError(f"Option '{name}' not found in default options.")

        # If the option is set in command line arguments, use that
        if name in self.cmd_line_args:
            value = self.cmd_line_args[name]
            if isinstance(self.default_options, bool) and isinstance(value, str) and value.lower() in ('true', 'false'):
                value = value.lower() == 'true'
            setattr(self.options, name, value)
            self.prompted_options.add(name)
            return value

        # Replace placeholders in the default value
        if isinstance(default_value, str):
            constains_placeholder = "<" in default_value and ">" in default_value
            if constains_placeholder:
                has_dir_key = False

                def repl(match):
                    nonlocal has_dir_key
                    key = match.group(1)
                    has_dir_key = has_dir_key or key.endswith("-dir")
                    key = key.replace("-", "_")
                    transform_fn = match.group(2)
                    if key == 'os':
                        if self.is_windows():
                            val = "windows"
                        elif self.is_linux():
                            val = "linux"
                        elif self.is_macos():
                            val = "macos"
                        else:
                            raise ValueError("Unsupported operating system.")
                    else:
                        val = getattr(self.options, key, None)
                    if transform_fn:
                        if transform_fn == "lower":
                            val = val.lower()
                        elif transform_fn == "upper":
                            val = val.upper()
                        # Add more formats as needed
                    return val

                # Regex: <key> or <key:format>
                pattern = r"<([a-zA-Z0-9_\-]+)(?::([a-zA-Z0-9_\-]+))?>"
                default_value = re.sub(pattern, repl, default_value)
                if has_dir_key:
                    default_value = os.path.abspath(default_value)
                setattr(self.default_options, name, default_value)

        # If it's non-interactive, use the default value directly
        if self.options.non_interactive:
            setattr(self.options, name, default_value)
            self.prompted_options.add(name)
            return default_value

        # Generate prompt to ask for value
        if name in INSTALL_OPTION_DESCRIPTIONS:
            prompt = INSTALL_OPTION_DESCRIPTIONS[name]
        else:
            raise ValueError(f"Missing description for option '{name}' in INSTALL_OPTION_DESCRIPTIONS.")
        # Prompt the user for the option value depending on the type
        if isinstance(getattr(self.default_options, name), bool):
            value = self.prompt_boolean(prompt, default_value)
        else:
            value = self.prompt_string(prompt, default_value)

        # Set the option and return the value
        setattr(self.options, name, value)
        self.prompted_options.add(name)
        return value

    def reprompt_option(self, name):
        return self.prompt_option(name, force_prompt=True)

    def prompt_build_type_option(self, name):
        value = self.prompt_option(name)
        valid_build_types = ["Debug", "Release", "RelWithDebInfo", "DebWithOpt", "MinSizeRel"]
        for t in valid_build_types:
            if t.lower() == value.lower():
                setattr(self.options, name, t)
                return value
        print(f"Invalid build type '{value}'. Must be one of: {', '.join(valid_build_types)}.")
        value = self.reprompt_option(name)
        for t in valid_build_types:
            if t.lower() == value.lower():
                setattr(self.options, name, t)
                return value
        print(f"Invalid build type '{value}'. Must be one of: {', '.join(valid_build_types)}.")
        raise ValueError(f"Invalid build type '{value}'. Must be one of: {', '.join(valid_build_types)}.")

    def supports_ansi(self):
        if os.name == "posix":
            return True
        if os.name == "nt":
            # Windows 10+ with VT enabled, or running in Windows Terminal
            return "WT_SESSION" in os.environ or sys.stdout.isatty()
        return False

    def run_cmd(self, cmd, cwd=None):
        """
        Runs a shell command in the specified directory, printing the command in blue if supported.
        """
        BLUE = "\033[94m"
        RESET = "\033[0m"
        if cwd is None:
            cwd = os.getcwd()
        color = BLUE if self.supports_ansi() else ""
        reset = RESET if self.supports_ansi() else ""
        if isinstance(cmd, list):
            cmd_str = ' '.join(shlex.quote(arg) for arg in cmd)
            print(f"{color}{cwd}> {cmd_str}{reset}")
        else:
            print(f"{color}{cwd}> {cmd}{reset}")
        r = subprocess.run(cmd, shell=isinstance(cmd, str), check=True, cwd=cwd)
        if r.returncode != 0:
            raise RuntimeError(f"Command '{cmd}' failed with return code {r.returncode}.")

    def clone_repo(self, repo, dest, branch=None, depth=None):
        """
        Clones a Git repository into the specified destination directory.
        :param repo: The URL of the repository to clone.
        :param dest: The destination directory where the repository will be cloned.
        :param branch: The branch or tag to checkout after cloning. Defaults to None (default branch).
        :param depth: The depth of the clone. Defaults to 1 (shallow clone).
        :return: None
        """
        cmd = [self.options.git_path, "clone"]
        if branch:
            cmd.extend(["--branch", branch])
        if depth:
            cmd.extend(["--depth", str(depth)])
        cmd.extend([repo, dest])
        self.run_cmd(cmd)

    def download_file(self, url, dest):
        """
        Downloads a file from the specified URL using Python's urllib.
        :param url: The URL of the file to download.
        :param dest: The destination file path where the file will be saved.
        :return: None
        """
        if os.path.exists(dest):
            print(f"File {dest} already exists. Skipping download.")
            return
        # Ensure the destination directory exists
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        print(f"Downloading {url} to {dest}")
        urllib.request.urlretrieve(url, dest)

    def is_windows(self):
        """
        Checks if the current operating system is Windows.
        :return: bool: True if the OS is Windows, False otherwise.
        """
        return os.name == "nt"

    def is_linux(self):
        """
        Checks if the current operating system is Linux.
        :return: bool: True if the OS is Linux, False otherwise.
        """
        return os.name == "posix" and sys.platform.startswith("linux")

    def is_macos(self):
        """
        Checks if the current operating system is macOS.
        :return: bool: True if the OS is macOS, False otherwise.
        """
        return os.name == "posix" and sys.platform.startswith("darwin")

    def cmake_workflow(self, src_dir, build_type, build_dir, install_dir, extra_args=None):
        """
        Configures and builds a CMake project.
        :param src: The source directory containing the CMakeLists.txt file.
        :param build: The build directory where the project will be built.
        :param extra_args: Additional arguments to pass to the CMake configuration command.
        :return: None
        """
        config_args = [self.options.cmake_path, "-S", src_dir]

        # "DebWithOpt" is not a valid type. However, we interpret it as a special case
        # where the build type is Debug and optimizations are enabled.
        # This is not very different from RelWithDebInfo on Unix, but ensures
        # Debug flags are used on Windows.
        build_type_is_debwithopt = build_type.lower() == 'debwithopt'
        cmake_build_type = build_type if not build_type_is_debwithopt else "Debug"
        if build_dir:
            config_args.extend(["-B", build_dir])
        if build_type:
            config_args.extend([f"-DCMAKE_BUILD_TYPE={cmake_build_type}"])
            if build_type_is_debwithopt:
                if self.is_windows():
                    config_args.extend(["-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /Ob1 /O2 /Zi",
                                        "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"])
                else:
                    config_args.extend(["-DCMAKE_CXX_FLAGS=-Og -g", "-DCMAKE_C_FLAGS=-Og -g"])
        if isinstance(extra_args, str):
            config_args.extend(extra_args.split())
        elif isinstance(extra_args, list):
            config_args.extend(extra_args)
        self.run_cmd(config_args)

        build_args = [self.options.cmake_path, "--build", build_dir, "--config", cmake_build_type]
        num_cores = os.cpu_count() or 1
        max_safe_parallel = 4  # Ideally 4GB per job
        build_args.extend(["--parallel", str(min(num_cores, max_safe_parallel))])
        self.run_cmd(build_args)

        install_args = [self.options.cmake_path, "--install", build_dir]
        if install_dir:
            install_args.extend(["--prefix", install_dir])
        if cmake_build_type:
            install_args.extend(["--config", cmake_build_type])
        self.run_cmd(install_args)

    def is_executable(self, path):
        if not os.path.isfile(path):
            return False
        if os.name == "nt":
            # On Windows, check for executable extensions
            _, ext = os.path.splitext(path)
            return ext.lower() in [".exe", ".bat", ".cmd", ".com"]
        else:
            return os.access(path, os.X_OK)

    def check_tool(self, tool):
        """
        Checks if the required tools are available as a command line argument or
        the system PATH.

        If the path is available as a command line argument {tool}-path, we check if the tool really
        exists.

        If the path is not available as a command line argument, we check if the tool
        exists in the system PATH.

        If any of these checks fail, an error is raised indicating the missing tool.

        :return: None
        """
        default_value = shutil.which(tool)
        setattr(self.default_options, f"{tool}_path", default_value)
        tool_path = self.prompt_option(f"{tool}_path")
        if not self.is_executable(tool_path):
            raise FileNotFoundError(f"{tool} executable not found at {tool_path}.")

    def check_tools(self):
        tools = ["git", "cmake"]
        for tool in tools:
            self.check_tool(tool)

    def setup_mrdocs_dir(self):
        self.prompt_option("mrdocs_src_dir")
        if not os.path.isabs(self.options.mrdocs_src_dir):
            self.options.mrdocs_src_dir = os.path.abspath(self.options.mrdocs_src_dir)
        if not os.path.exists(self.options.mrdocs_src_dir):
            if not self.prompt_boolean(
                    f"Source directory '{self.options.mrdocs_src_dir}' does not exist. Create and clone MrDocs there?",
                    True):
                print("Installation aborted by user.")
                return
            self.prompt_option("mrdocs_branch")
            self.prompt_option("mrdocs_repo")
            self.clone_repo(self.options.mrdocs_repo, self.options.mrdocs_src_dir, branch=self.options.mrdocs_branch)
        else:
            if not os.path.isdir(self.options.mrdocs_src_dir):
                raise NotADirectoryError(
                    f"Specified mrdocs_src_dir '{self.options.mrdocs_src_dir}' is not a directory.")

        # MrDocs build type
        self.prompt_build_type_option("mrdocs_build_type")
        self.prompt_option("mrdocs_build_tests")

    def is_inside_mrdocs_dir(self, path):
        """
        Checks if the given path is inside the MrDocs source directory.
        :param path: The path to check.
        :return: bool: True if the path is inside the MrDocs source directory, False otherwise.
        """
        return os.path.commonpath([self.options.mrdocs_src_dir, path]) == self.options.mrdocs_src_dir

    def prompt_dependency_path_option(self, name):
        """
        Prompts the user for a dependency path option, ensuring it is not inside the MrDocs source directory.
        :param name: The name of the option to prompt for.
        :return: The value of the option after prompting the user.
        """
        self.prompt_option(name)
        value = getattr(self.options, name)
        value = os.path.abspath(value)
        setattr(self.options, name, value)
        while self.is_inside_mrdocs_dir(value):
            print(f"Error: {name} '{value}' cannot be inside mrdocs_src_dir '{self.options.mrdocs_src_dir}'.")
            if not self.prompt_boolean(f"Would you like to specify a different {name} directory."):
                print("Installation aborted by user.")
                return
            value = self.reprompt_option(name)
            setattr(self.options, name, value)

        if not os.path.exists(value):
            if not self.prompt_boolean(f"'{value}' does not exist. Create it?", True):
                raise FileNotFoundError(f"'{value}' does not exist and user chose not to create it.")
        return value

    def setup_third_party_dir(self):
        self.prompt_dependency_path_option("third_party_src_dir")
        os.makedirs(self.options.third_party_src_dir, exist_ok=True)

    def install_duktape(self):
        self.prompt_dependency_path_option("duktape_src_dir")
        if not os.path.exists(self.options.duktape_src_dir):
            self.prompt_option("duktape_url")
            archive_filename = os.path.basename(self.options.duktape_url)
            archive_path = os.path.join(self.options.third_party_src_dir, archive_filename)
            self.download_file(self.options.duktape_url, archive_path)
            with tarfile.open(archive_path, "r:xz") as tar:
                top_level = tar.getnames()[0].split('/')[0]
                for member in tar.getmembers():
                    # Remove the top-level directory from the path
                    member_path = os.path.relpath(member.name, top_level)
                    if member_path == '.':
                        continue
                    member.name = member_path
                    tar.extract(member, path=self.options.duktape_src_dir)
        duktape_patches = os.path.join(self.options.mrdocs_src_dir, 'third-party', 'duktape')
        if os.path.exists(duktape_patches):
            for patch_file in os.listdir(duktape_patches):
                patch_path = os.path.join(duktape_patches, patch_file)
                shutil.copy(patch_path, self.options.duktape_src_dir)
        duk_config_path = os.path.join(self.options.duktape_src_dir, "src", "duk_config.h")
        if os.path.exists(duk_config_path):
            with open(duk_config_path, "r") as f:
                content = f.read()
            new_content = content.replace("#define DUK_F_DLL_BUILD", "#undef DUK_F_DLL_BUILD")
            if new_content != content:
                with open(duk_config_path, "w") as f:
                    f.write(new_content)
        else:
            print(f"Warning: {duk_config_path} does not exist. Skipping patch.")
        self.prompt_build_type_option("duktape_build_type")
        self.prompt_dependency_path_option("duktape_build_dir")
        self.prompt_dependency_path_option("duktape_install_dir")
        self.cmake_workflow(self.options.duktape_src_dir, self.options.duktape_build_type,
                            self.options.duktape_build_dir, self.options.duktape_install_dir)

    def install_libxml2(self):
        self.prompt_dependency_path_option("libxml2_src_dir")
        if not os.path.exists(self.options.libxml2_src_dir):
            self.prompt_option("libxml2_repo")
            self.prompt_option("libxml2_branch")
            self.clone_repo(self.options.libxml2_repo, self.options.libxml2_src_dir, branch=self.options.libxml2_branch,
                            depth=1)
        self.prompt_build_type_option("libxml2_build_type")
        self.prompt_dependency_path_option("libxml2_build_dir")
        self.prompt_dependency_path_option("libxml2_install_dir")
        extra_args = [
            "-DBUILD_SHARED_LIBS=OFF",
            "-DLIBXML2_WITH_PROGRAMS=ON",
            "-DLIBXML2_WITH_FTP=OFF",
            "-DLIBXML2_WITH_HTTP=OFF",
            "-DLIBXML2_WITH_ICONV=OFF",
            "-DLIBXML2_WITH_LEGACY=OFF",
            "-DLIBXML2_WITH_LZMA=OFF",
            "-DLIBXML2_WITH_ZLIB=OFF",
            "-DLIBXML2_WITH_ICU=OFF",
            "-DLIBXML2_WITH_TESTS=OFF",
            "-DLIBXML2_WITH_HTML=ON",
            "-DLIBXML2_WITH_C14N=ON",
            "-DLIBXML2_WITH_CATALOG=ON",
            "-DLIBXML2_WITH_DEBUG=ON",
            "-DLIBXML2_WITH_ISO8859X=ON",
            "-DLIBXML2_WITH_MEM_DEBUG=OFF",
            "-DLIBXML2_WITH_MODULES=ON",
            "-DLIBXML2_WITH_OUTPUT=ON",
            "-DLIBXML2_WITH_PATTERN=ON",
            "-DLIBXML2_WITH_PUSH=ON",
            "-DLIBXML2_WITH_PYTHON=OFF",
            "-DLIBXML2_WITH_READER=ON",
            "-DLIBXML2_WITH_REGEXPS=ON",
            "-DLIBXML2_WITH_SAX1=ON",
            "-DLIBXML2_WITH_SCHEMAS=ON",
            "-DLIBXML2_WITH_SCHEMATRON=ON",
            "-DLIBXML2_WITH_THREADS=ON",
            "-DLIBXML2_WITH_THREAD_ALLOC=OFF",
            "-DLIBXML2_WITH_TREE=ON",
            "-DLIBXML2_WITH_VALID=ON",
            "-DLIBXML2_WITH_WRITER=ON",
            "-DLIBXML2_WITH_XINCLUDE=ON",
            "-DLIBXML2_WITH_XPATH=ON",
            "-DLIBXML2_WITH_XPTR=ON"
        ]
        self.cmake_workflow(self.options.libxml2_src_dir, self.options.libxml2_build_type,
                            self.options.libxml2_build_dir, self.options.libxml2_install_dir, extra_args)

    def install_llvm(self):
        self.prompt_dependency_path_option("llvm_src_dir")
        if not os.path.exists(self.options.llvm_src_dir):
            self.prompt_option("llvm_repo")
            self.prompt_option("llvm_commit")
            os.makedirs(self.options.llvm_src_dir, exist_ok=True)
            self.run_cmd("git init", self.options.llvm_src_dir)
            self.run_cmd(f"git remote add origin {self.options.llvm_repo}", self.options.llvm_src_dir)
            self.run_cmd(f"git fetch --depth 1 origin {self.options.llvm_commit}", self.options.llvm_src_dir)
            self.run_cmd("git checkout FETCH_HEAD", self.options.llvm_src_dir)

        llvm_subproject_dir = os.path.join(self.options.llvm_src_dir, "llvm")
        llvm_patches = os.path.join(self.options.mrdocs_src_dir, 'third-party', 'llvm')
        if os.path.exists(llvm_patches):
            for patch_file in os.listdir(llvm_patches):
                patch_path = os.path.join(llvm_patches, patch_file)
                shutil.copy(patch_path, llvm_subproject_dir)
        self.prompt_build_type_option("llvm_build_type")
        self.prompt_dependency_path_option("llvm_build_dir")
        self.prompt_dependency_path_option("llvm_install_dir")
        cmake_preset = f"{self.options.llvm_build_type.lower()}-win" if self.is_windows() else f"{self.options.llvm_build_type.lower()}-unix"
        cmake_extra_args = [f"--preset={cmake_preset}"]
        if self.is_windows():
            cmake_extra_args.append("-DLLVM_ENABLE_RUNTIMES=libcxx")
        else:
            cmake_extra_args.append("-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi;libunwind")
        self.cmake_workflow(llvm_subproject_dir, self.options.llvm_build_type, self.options.llvm_build_dir,
                            self.options.llvm_install_dir, cmake_extra_args)

    def create_cmake_presets(self):
        # Ask the user if they want to create CMake User presets referencing the installed dependencies
        # Otherwise, we skip this step and pass the directories as command line arguments to the CMake build command
        if not self.prompt_option("mrdocs_use_user_presets"):
            print("Skipping CMake User presets creation as per user preference.")
            return

        # If they choose to create presets, we either generate or update the CMakeUserPresets.json file
        user_presets_path = os.path.join(self.options.mrdocs_src_dir, "CMakeUserPresets.json")
        if not os.path.exists(user_presets_path):
            user_presets_example_path = os.path.join(self.options.mrdocs_src_dir, "CMakeUserPresets.json.example")
            if not os.path.exists(user_presets_example_path):
                raise FileNotFoundError(f"Cannot find CMakeUserPresets.json.example in {self.options.mrdocs_src_dir}.")
            shutil.copy(user_presets_example_path, user_presets_path)

        # Now that we know the file exists, we can read it and update the paths
        # Read the file as json
        with open(user_presets_path, "r") as f:
            user_presets = json.load(f)

        # Come up with a nice user preset name
        self.prompt_option("mrdocs_preset_name")

        # Upsert the preset in the "configurePresets" array of objects
        # If preset with the same name already exists, we update it
        # If a preset with the same name does not exist, we create it
        hostSystemName = "Windows"
        if os.name == "posix":
            if os.uname().sysname == "Darwin":
                hostSystemName = "Darwin"
            else:
                hostSystemName = "Linux"
        OSDisplayName = hostSystemName
        if OSDisplayName == "Darwin":
            OSDisplayName = "macOS"
        parent_preset_name = "debug"
        if self.options.mrdocs_build_type.lower() == "release":
            parent_preset_name = "release"
        elif self.options.mrdocs_build_type.lower() == "relwithdebinfo":
            parent_preset_name = "relwithdebinfo"
        build_type_is_debwithopt = self.options.mrdocs_build_type.lower() == 'debwithopt'
        cmake_build_type = self.options.mrdocs_build_type if not build_type_is_debwithopt else "Debug"
        new_preset = {
            "name": self.options.mrdocs_preset_name,
            "displayName": f"{self.options.mrdocs_build_type} {OSDisplayName}",
            "description": f"Preset for building MrDocs in {self.options.mrdocs_build_type} mode with the default compiler in {OSDisplayName}.",
            "inherits": parent_preset_name,
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": cmake_build_type,
                "LLVM_ROOT": self.options.llvm_install_dir,
                "Clang_ROOT": self.options.llvm_install_dir,
                "duktape_ROOT": self.options.duktape_install_dir,
                "Duktape_ROOT": self.options.duktape_install_dir,
                "libxml2_ROOT": self.options.libxml2_install_dir,
                "LibXml2_ROOT": self.options.libxml2_install_dir,
                "MRDOCS_BUILD_TESTS": self.options.mrdocs_build_tests,
                "MRDOCS_BUILD_DOCS": False,
                "MRDOCS_GENERATE_REFERENCE": False,
                "MRDOCS_GENERATE_ANTORA_REFERENCE": False
            },
            "warnings": {
                "unusedCli": False
            },
            "condition": {
                "type": "equals",
                "lhs": "${hostSystemName}",
                "rhs": hostSystemName
            }
        }

        # Iterate the cacheVariables and,
        # 1) If any starts with the value of the parent of mrdocs-src-dir, we replace it with ${sourceParentDir} to make it relative
        # 2) If any starts with the value of mrdocs-src-dir, we replace it with ${sourceDir} to make it relative
        # 3) if any starts with the value of $HOME, we replace it with $env{HOME} to make it relative
        mrdocs_src_dir_parent = os.path.dirname(self.options.mrdocs_src_dir)
        if mrdocs_src_dir_parent == self.options.mrdocs_src_dir:
            mrdocs_src_dir_parent = ''
        mrdocs_home_dir = os.path.expanduser("~")
        for key, value in new_preset["cacheVariables"].items():
            if not isinstance(value, str):
                continue
            # Replace mrdocs-src-dir parent with ${sourceParentDir}
            if mrdocs_src_dir_parent and value.startswith(mrdocs_src_dir_parent):
                new_value = "${sourceParentDir}" + value[len(mrdocs_src_dir_parent):]
                new_preset["cacheVariables"][key] = new_value
            # Replace mrdocs-src-dir with ${sourceDir}
            elif self.options.mrdocs_src_dir and value.startswith(self.options.mrdocs_src_dir):
                new_value = "${sourceDir}" + value[len(self.options.mrdocs_src_dir):]
                new_preset["cacheVariables"][key] = new_value
            # Replace $HOME with $env{HOME}
            elif mrdocs_home_dir and value.startswith(mrdocs_home_dir):
                new_value = "$env{HOME}" + value[len(mrdocs_home_dir):]
                new_preset["cacheVariables"][key] = new_value

        preset_exists = False
        for preset in user_presets.get("configurePresets", []):
            if preset.get("name") == self.options.mrdocs_preset_name:
                preset_exists = True
                # Update the existing preset
                preset.update(new_preset)
                break
        if not preset_exists:
            # Add the new preset to the list
            user_presets.setdefault("configurePresets", []).append(new_preset)

        # Write the updated presets back to the file
        with open(user_presets_path, "w") as f:
            json.dump(user_presets, f, indent=4)

    def install_mrdocs(self):
        if not self.options.mrdocs_use_user_presets:
            # Build directory specified in the preset
            self.prompt_option("mrdocs_build_dir")
        else:
            self.options.mrdocs_build_dir = os.path.join(self.options.mrdocs_src_dir, "build",
                                                         self.options.mrdocs_preset_name)
            self.default_options.mrdocs_build_dir = self.options.mrdocs_build_dir

        if not self.prompt_option("mrdocs_system_install"):
            # Build directory specified in the preset
            # self.prompt_option("mrdocs_build_dir")
            self.prompt_option("mrdocs_install_dir")

        extra_args = []
        if not self.options.mrdocs_system_install and self.options.mrdocs_install_dir:
            extra_args.extend(["-D", f"CMAKE_INSTALL_PREFIX={self.options.mrdocs_install_dir}"])

        if self.options.mrdocs_use_user_presets:
            extra_args.append(f"--preset={self.options.mrdocs_preset_name}")
        else:
            # If not using user presets, we pass the directories as command line arguments
            extra_args.extend([
                "-D", f"LLVM_ROOT={self.options.llvm_install_dir}",
                "-D", f"Clang_ROOT={self.options.llvm_install_dir}",
                "-D", f"duktape_ROOT={self.options.duktape_install_dir}",
                "-D", f"Duktape_ROOT={self.options.duktape_install_dir}",
            ])
            if self.options.mrdocs_build_tests:
                extra_args.extend([
                    "-D", f"libxml2_ROOT={self.options.libxml2_install_dir}",
                    "-D", f"LibXml2_ROOT={self.options.libxml2_install_dir}"
                ])
                extra_args.extend(["-D", "MRDOCS_BUILD_TESTS=ON"])
            extra_args.extend(["-DMRDOCS_BUILD_DOCS=OFF", "-DMRDOCS_GENERATE_REFERENCE=OFF",
                               "-DMRDOCS_GENERATE_ANTORA_REFERENCE=OFF"])

        self.cmake_workflow(self.options.mrdocs_src_dir, self.options.mrdocs_build_type, self.options.mrdocs_build_dir,
                            self.options.mrdocs_install_dir, extra_args)
        if self.options.mrdocs_build_dir and self.prompt_option("mrdocs_run_tests"):
            # Look for ctest path relative to the cmake path
            ctest_path = os.path.join(os.path.dirname(self.options.cmake_path), "ctest")
            if not os.path.exists(ctest_path):
                raise FileNotFoundError(
                    f"ctest executable not found at {ctest_path}. Please ensure CMake is installed correctly.")
            test_args = [ctest_path, "--test-dir", self.options.mrdocs_build_dir, "--output-on-failure", "--progress",
                         "--no-tests=error", "--output-on-failure", "--parallel", str(os.cpu_count() or 1)]
            self.run_cmd(test_args)

        YELLOW = "\033[93m"
        RESET = "\033[0m"
        if self.supports_ansi():
            print(f"{YELLOW}MrDocs has been successfully installed in {self.options.mrdocs_install_dir}.{RESET}")
        else:
            print(f"\nMrDocs has been successfully installed in {self.options.mrdocs_install_dir}.\n")

    def generate_clion_run_configs(self, configs):
        import xml.etree.ElementTree as ET

        # Generate CLion run configurations for MrDocs
        # For each configuration, we create an XML file in <mrdocs-src-dir>/.run
        # named <config-name>.run.xml
        run_dir = os.path.join(self.options.mrdocs_src_dir, ".run")
        os.makedirs(run_dir, exist_ok=True)
        for config in configs:
            config_name = config["name"]
            run_config_path = os.path.join(run_dir, f"{config_name}.run.xml")
            root = ET.Element("component", name="ProjectRunConfigurationManager")
            if 'target' in config:
                clion_config = ET.SubElement(root, "configuration", {
                    "default": "false",
                    "name": config["name"],
                    "type": "CMakeRunConfiguration",
                    "factoryName": "Application",
                    "PROGRAM_PARAMS": ' '.join(shlex.quote(arg) for arg in config["args"]),
                    "REDIRECT_INPUT": "false",
                    "ELEVATE": "false",
                    "USE_EXTERNAL_CONSOLE": "false",
                    "EMULATE_TERMINAL": "false",
                    "PASS_PARENT_ENVS_2": "true",
                    "PROJECT_NAME": "MrDocs",
                    "TARGET_NAME": config["target"],
                    "CONFIG_NAME": self.options.mrdocs_preset_name or "debug",
                    "RUN_TARGET_PROJECT_NAME": "MrDocs",
                    "RUN_TARGET_NAME": config["target"]
                })
                method = ET.SubElement(clion_config, "method", v="2")
                ET.SubElement(method, "option",
                              name="com.jetbrains.cidr.execution.CidrBuildBeforeRunTaskProvider$BuildBeforeRunTask",
                              enabled="true")
            elif 'script' in config:
                if config["script"].endswith(".py"):
                    clion_config = ET.SubElement(root, "configuration", {
                        "default": "false",
                        "name": config["name"],
                        "type": "PythonConfigurationType",
                        "factoryName": "Python",
                        "nameIsGenerated": "false"
                    })
                    ET.SubElement(clion_config, "module", name="mrdocs")
                    ET.SubElement(clion_config, "option", name="ENV_FILES", value="")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="PARENT_ENVS", value="true")
                    envs = ET.SubElement(clion_config, "envs")
                    ET.SubElement(envs, "env", name="PYTHONUNBUFFERED", value="1")
                    ET.SubElement(clion_config, "option", name="SDK_HOME", value="")
                    if 'cwd' in config and config["cwd"] != self.options.mrdocs_src_dir:
                        ET.SubElement(clion_config, "option", name="WORKING_DIRECTORY", value=config["cwd"])
                    else:
                        ET.SubElement(clion_config, "option", name="WORKING_DIRECTORY", value="$PROJECT_DIR$")
                    ET.SubElement(clion_config, "option", name="IS_MODULE_SDK", value="true")
                    ET.SubElement(clion_config, "option", name="ADD_CONTENT_ROOTS", value="true")
                    ET.SubElement(clion_config, "option", name="ADD_SOURCE_ROOTS", value="true")
                    ET.SubElement(clion_config, "option", name="SCRIPT_NAME", value=config["script"])
                    ET.SubElement(clion_config, "option", name="PARAMETERS",
                                  value=' '.join(shlex.quote(arg) for arg in config["args"]))
                    ET.SubElement(clion_config, "option", name="SHOW_COMMAND_LINE", value="false")
                    ET.SubElement(clion_config, "option", name="EMULATE_TERMINAL", value="false")
                    ET.SubElement(clion_config, "option", name="MODULE_MODE", value="false")
                    ET.SubElement(clion_config, "option", name="REDIRECT_INPUT", value="false")
                    ET.SubElement(clion_config, "option", name="INPUT_FILE", value="")
                    ET.SubElement(clion_config, "method", v="2")
                elif config["script"].endswith(".sh"):
                    clion_config = ET.SubElement(root, "configuration", {
                        "default": "false",
                        "name": config["name"],
                        "type": "ShConfigurationType"
                    })
                    ET.SubElement(clion_config, "option", name="SCRIPT_TEXT", value=f"bash {shlex.quote(config['script'])}")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_PATH", value="true")
                    ET.SubElement(clion_config, "option", name="SCRIPT_PATH", value=config["script"])
                    ET.SubElement(clion_config, "option", name="SCRIPT_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_SCRIPT_WORKING_DIRECTORY", value="true")
                    if 'cwd' in config and config["cwd"] != self.options.mrdocs_src_dir:
                        ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value=config["cwd"])
                    else:
                        ET.SubElement(clion_config, "option", name="SCRIPT_WORKING_DIRECTORY", value="$PROJECT_DIR$")
                    ET.SubElement(clion_config, "option", name="INDEPENDENT_INTERPRETER_PATH", value="true")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_PATH", value="")
                    ET.SubElement(clion_config, "option", name="INTERPRETER_OPTIONS", value="")
                    ET.SubElement(clion_config, "option", name="EXECUTE_IN_TERMINAL", value="true")
                    ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="true")
                    ET.SubElement(clion_config, "envs")
                    ET.SubElement(clion_config, "method", v="2")
            tree = ET.ElementTree(root)
            tree.write(run_config_path, encoding="utf-8", xml_declaration=False)

    def generate_visual_studio_run_configs(self, configs):
        # Visual Studio launch configs are stored in .vs/launch.vs.json
        vs_dir = os.path.join(self.options.mrdocs_src_dir, ".vs")
        os.makedirs(vs_dir, exist_ok=True)
        launch_path = os.path.join(vs_dir, "launch.vs.json")

        # Load existing configs if present
        if os.path.exists(launch_path):
            with open(launch_path, "r") as f:
                launch_data = json.load(f)
        else:
            launch_data = {"version": "0.2.1", "configurations": []}

        # Build a dict for quick lookup by name
        vs_configs_by_name = {cfg.get("name"): cfg for cfg in launch_data.get("configurations", [])}

        for config in configs:
            new_cfg = {
                "name": config["name"],
                "type": "default",
                "project": "MrDocs",
                "args": config["args"],
                "cwd": self.options.mrdocs_build_dir,
                "env": {},
                "stopAtEntry": False,
                "console": "integratedTerminal"
            }

            if 'target' in config:
                new_cfg["projectTarget"] = config["target"]
            if 'script' in config:
                new_cfg["program"] = config["script"]

            # Replace or add
            vs_configs_by_name[config["name"]] = new_cfg

        # Write back all configs
        launch_data["configurations"] = list(vs_configs_by_name.values())
        with open(launch_path, "w") as f:
            json.dump(launch_data, f, indent=4)

    def generate_run_configs(self):
        # Configurations using MrDocs executable
        configs = [{
            "name": "MrDocs Version",
            "target": "mrdocs",
            "args": ["--version"]
        }, {
            "name": "MrDocs Help",
            "target": "mrdocs",
            "args": ["--help"]
        }]

        # Configuration to run unit tests
        if self.options.mrdocs_build_tests:
            configs.append({
                "name": "MrDocs Unit Tests",
                "target": "mrdocs-test",
                "args": [
                    '--unit=true'
                ]
            })

        # Configurations to Update/Test/Create test fixtures
        for verb in ["update", "test", "create"]:
            for generator in ["adoc", "html", "xml"]:
                configs.append({
                    "name": f"MrDocs {verb.title()} Test Fixtures ({generator.upper()})",
                    "target": "mrdocs-test",
                    "args": [
                        f'"{self.options.mrdocs_src_dir}/test-files/golden-tests"',
                        '--unit=false',
                        f'--action={verb}',
                        f'--generator={generator}',
                        f'--addons="{self.options.mrdocs_src_dir}/share/mrdocs/addons"',
                        f'--stdlib-includes="{self.options.llvm_install_dir}/include/c++/v1"',
                        f'--stdlib-includes="{self.options.llvm_install_dir}/lib/clang/20/include"',
                        f'--libc-includes="{self.options.mrdocs_src_dir}/share/mrdocs/headers/libc-stubs"',
                        '--log-level=warn'
                    ]
                })

        self.prompt_option("boost_src_dir")
        if self.options.boost_src_dir and os.path.exists(self.options.boost_src_dir):
            boost_libs = os.path.join(self.options.boost_src_dir, 'libs')
            if os.path.exists(boost_libs):
                for lib in os.listdir(boost_libs):
                    mrdocs_config = os.path.join(boost_libs, lib, 'doc', 'mrdocs.yml')
                    if os.path.exists(mrdocs_config):
                        print(f"Generating run configuration for Boost library '{lib}'")
                        configs.append({
                            "name": f"Boost.{lib.title()} Documentation",
                            "target": "mrdocs",
                            "args": [
                                '"../CMakeLists.txt"',
                                f'--config="{self.options.boost_src_dir}/libs/{lib}/doc/mrdocs.yml"',
                                f'--output="{self.options.boost_src_dir}/libs/{lib}/doc/modules/reference/pages"',
                                f'--generator=adoc',
                                f'--addons="{self.options.mrdocs_src_dir}/share/mrdocs/addons"',
                                f'--stdlib-includes="{self.options.llvm_install_dir}/include/c++/v1"',
                                f'--stdlib-includes="{self.options.llvm_install_dir}/lib/clang/20/include"',
                                f'--libc-includes="{self.options.mrdocs_src_dir}/share/mrdocs/headers/libc-stubs"',
                                f'--tagfile=reference.tag.xml',
                                '--multipage=true',
                                '--concurrency=32',
                                '--log-level=debug'
                            ]
                        })
            else:
                print(
                    f"Warning: Boost source directory '{self.options.boost_src_dir}' does not contain 'libs' directory. Skipping Boost documentation target generation.")

        # bootstrap.py targets
        configs.append({
            "name": f"MrDocs Bootstrap Help",
            "script": os.path.join(self.options.mrdocs_src_dir, "bootstrap.py"),
            "args": ["--help"],
            "cwd": self.options.mrdocs_src_dir
        })

        bootstrap_args = []
        for field in dataclasses.fields(InstallOptions):
            value = getattr(self.options, field.name)
            default_value = getattr(self.default_options, field.name, None)
            if value is not None and (value != default_value or field.name == 'mrdocs_build_type'):
                if field.name == 'non_interactive':
                    # Skip non_interactive as it is handled separately,
                    continue
                if field.type is bool:
                    if value:
                        bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                    else:
                        bootstrap_args.append(f"--no-{field.name.replace('_', '-')}")
                elif field.type is str:
                    if value != '' and default_value != '':
                        bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                        bootstrap_args.append(value)
                else:
                    raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")
        bootstrap_refresh_config_name = self.options.mrdocs_preset_name or self.options.mrdocs_build_type or "debug"
        configs.append({
            "name": f"MrDocs Bootstrap Update ({bootstrap_refresh_config_name})",
            "script": os.path.join(self.options.mrdocs_src_dir, "bootstrap.py"),
            "args": bootstrap_args,
            "cwd": self.options.mrdocs_src_dir
        })
        bootstrap_refresh_args = bootstrap_args.copy()
        bootstrap_refresh_args.append("--non-interactive")
        configs.append({
            "name": f"MrDocs Bootstrap Refresh ({bootstrap_refresh_config_name})",
            "script": os.path.join(self.options.mrdocs_src_dir, "bootstrap.py"),
            "args": bootstrap_refresh_args,
            "cwd": self.options.mrdocs_src_dir
        })

        # Targets for the pre-build steps
        configs.append({
            "name": f"MrDocs Generate Config Info ({bootstrap_refresh_config_name})",
            "script": os.path.join(self.options.mrdocs_src_dir, 'util', 'generate-config-info.py'),
            "args": [os.path.join(self.options.mrdocs_src_dir, 'src', 'lib', 'ConfigOptions.json'),
                     os.path.join(self.options.mrdocs_build_dir)],
            "cwd": self.options.mrdocs_src_dir
        })
        configs.append({
            "name": f"MrDocs Generate YAML Schema",
            "script": os.path.join(self.options.mrdocs_src_dir, 'util', 'generate-yaml-schema.py'),
            "args": [],
            "cwd": self.options.mrdocs_src_dir
        })

        # Documentation generation targets
        mrdocs_docs_dir = os.path.join(self.options.mrdocs_src_dir, "docs")
        mrdocs_docs_ui_dir = os.path.join(mrdocs_docs_dir, "ui")
        mrdocs_docs_script_ext = "bat" if self.is_windows() else "sh"
        configs.append({
            "name": "MrDocs Build Local Docs",
            "script": os.path.join(mrdocs_docs_dir, f"build_local_docs.{mrdocs_docs_script_ext}"),
            "args": [],
            "cwd": mrdocs_docs_dir
        })
        configs.append({
            "name": "MrDocs Build Docs",
            "script": os.path.join(mrdocs_docs_dir, f"build_docs.{mrdocs_docs_script_ext}"),
            "args": [],
            "cwd": mrdocs_docs_dir
        })
        configs.append({
            "name": "MrDocs Build UI Bundle",
            "script": os.path.join(mrdocs_docs_ui_dir, f"build.{mrdocs_docs_script_ext}"),
            "args": [],
            "cwd": mrdocs_docs_ui_dir
        })

        # Remove bad test files
        test_files_dir = os.path.join(self.options.mrdocs_src_dir, "test-files", "golden-tests")
        configs.append({
            "name": "MrDocs Remove Bad Test Files",
            "script": os.path.join(test_files_dir, f"remove_bad_files.{mrdocs_docs_script_ext}"),
            "args": [],
            "cwd": test_files_dir
        })

        print("Generating CLion run configurations for MrDocs...")
        self.generate_clion_run_configs(configs)
        print("Generating Visual Studio run configurations for MrDocs...")
        self.generate_visual_studio_run_configs(configs)

    def install_all(self):
        self.check_tools()
        self.setup_mrdocs_dir()
        self.setup_third_party_dir()
        self.install_duktape()
        if self.prompt_option("mrdocs_build_tests"):
            self.install_libxml2()
        self.install_llvm()
        self.create_cmake_presets()
        self.install_mrdocs()
        if self.prompt_option("generate_run_configs"):
            self.generate_run_configs()
        else:
            print("Skipping run configurations generation as per user preference.")


def get_command_line_args():
    """
    Parses command line arguments and returns them as a dictionary.

    Every field in the InstallOptions dataclass is converted to a
    valid command line argument description.

    :return: dict: Dictionary of command line arguments.
    """
    parser = argparse.ArgumentParser(
        description="Download and install MrDocs from source.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    for field in dataclasses.fields(InstallOptions):
        arg_name = f"--{field.name.replace('_', '-')}"
        help_text = INSTALL_OPTION_DESCRIPTIONS.get(field.name)
        if help_text is None:
            raise ValueError(f"Missing description for option '{field.name}' in INSTALL_OPTION_DESCRIPTIONS.")
        if field.default is not dataclasses.MISSING and field.default is not None:
            # if string
            if isinstance(field.default, str) and field.default:
                help_text += f" (default: '{field.default}')"
            elif field.default is True:
                help_text += " (default: true)"
            elif field.default is False:
                help_text += " (default: false)"
            else:
                help_text += f" (default: {field.default})"
        if field.type is bool:
            parser.add_argument(arg_name, dest=field.name, action='store_true', help=help_text, default=None)
            parser.add_argument(f"--no-{field.name.replace('_', '-')}", dest=field.name, action='store_false',
                                help=f'Set {arg_name} to false', default=None)
        elif field.type is str:
            parser.add_argument(arg_name, type=field.type, help=help_text, default=None)
        else:
            raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")
    return {k: v for k, v in vars(parser.parse_args()).items() if v is not None}


def main():
    args = get_command_line_args()
    installer = MrDocsInstaller(args)
    installer.install_all()


if __name__ == "__main__":
    main()
