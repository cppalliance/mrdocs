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
import platform
import subprocess
import os
import sys
import shutil
from dataclasses import dataclass, field
import dataclasses
import urllib.request
import tarfile
import json
import re

def get_default_mrdocs_src_dir():
    """
    Returns the default source directory for MrDocs based on the current working directory.

    If the current working directory is the same as the script directory, it
    means we're working on development. The script is being called from a location
    that's already a MrDocs repository. So the user wants the current directory
    to be used as the source directory, not to create a new source directory
    every time the script is run.

    If the current working directory is different from the script directory,
    it means we're running the script from a different location, likely a
    pre-existing MrDocs repository. In this case, we assume the user wants to
    create a new source directory for MrDocs in the current working directory
    under the name "mrdocs" or anywhere else in order to avoid conflicts
    and we assume the user is running this whole procedure to bootstrap,
    build, and install MrDocs from scratch.

    :return: str: The default source directory for MrDocs.
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    cwd = os.getcwd()
    if cwd == script_dir:
        return cwd
    else:
        return os.path.join(cwd, "mrdocs")


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
    mrdocs_src_dir: str = field(default_factory=get_default_mrdocs_src_dir)
    mrdocs_build_type: str = "Release"
    mrdocs_repo: str = "https://github.com/cppalliance/mrdocs"
    mrdocs_branch: str = "develop"
    mrdocs_use_user_presets: bool = True
    mrdocs_preset_name: str = "<mrdocs-build-type:lower>-<os:lower>"
    mrdocs_build_dir: str = "<mrdocs-src-dir>/build/<mrdocs-build-type:lower>-<os:lower>"
    mrdocs_build_tests: bool = True
    mrdocs_install_dir: str = "<mrdocs-src-dir>/install/<mrdocs-build-type:lower>-<os:lower>"
    mrdocs_system_install: bool = False
    mrdocs_run_tests: bool = True

    # Third-party dependencies
    third_party_src_dir: str = "<mrdocs-src-dir>/../third-party"

    # Fmt
    fmt_src_dir: str = "<third-party-src-dir>/fmt"
    fmt_build_type: str = "<mrdocs-build-type>"
    fmt_build_dir: str = "<fmt-src-dir>/build/<fmt-build-type:lower>"
    fmt_install_dir: str = "<fmt-src-dir>/install/<fmt-build-type:lower>"
    fmt_repo: str = "https://github.com/fmtlib/fmt"
    fmt_branch: str = "10.2.1"

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
    llvm_build_dir: str = "<llvm-src-dir>/llvm/build/<llvm-build-type:lower>"
    llvm_install_dir: str = "<llvm-src-dir>/install/<llvm-build-type:lower>"
    llvm_repo: str = "https://github.com/llvm/llvm-project.git"
    llvm_commit: str = "dd7a3d4d798e30dfe53b5bbbbcd9a23c24ea1af9"

    # Meta
    non_interactive: bool = False

# Constant for option descriptions
INSTALL_OPTION_DESCRIPTIONS = {
    "git_path": "Path to the git executable, if not in system PATH.",
    "cmake_path": "Path to the cmake executable, if not in system PATH.",
    "mrdocs_src_dir": "MrDocs source directory.",
    "mrdocs_repo": "URL of the MrDocs repository to clone.",
    "mrdocs_branch": "Branch or tag of the MrDocs repository to use.",
    "mrdocs_build_type": "CMake build type for MrDocs (e.g., Release, Debug).",
    "mrdocs_use_user_presets": "Whether to use CMake User Presets for building MrDocs.",
    "mrdocs_preset_name": "Name of the CMake User Preset to use for MrDocs.",
    "mrdocs_build_dir": "Directory where MrDocs will be built.",
    "mrdocs_build_tests": "Whether to build tests for MrDocs.",
    "mrdocs_install_dir": "Directory where MrDocs will be installed.",
    "mrdocs_system_install": "Whether to install MrDocs to the system directories instead of a custom install directory.",
    "mrdocs_run_tests": "Whether to run tests after building MrDocs.",
    "third_party_src_dir": "Directory for all third-party source dependencies.",
    "fmt_src_dir": "Directory for the fmt library source code.",
    "fmt_build_type": "CMake build type for the fmt library.",
    "fmt_build_dir": "Directory where the fmt library will be built.",
    "fmt_install_dir": "Directory where the fmt library will be installed.",
    "fmt_repo": "URL of the fmt library repository to clone.",
    "fmt_branch": "Branch or tag of the fmt library to use.",
    "duktape_src_dir": "Directory for the Duktape source code.",
    "duktape_url": "Download URL for the Duktape source archive.",
    "duktape_build_type": "CMake build type for Duktape.",
    "duktape_build_dir": "Directory where Duktape will be built.",
    "duktape_install_dir": "Directory where Duktape will be installed.",
    "libxml2_src_dir": "Directory for the libxml2 source code.",
    "libxml2_build_type": "CMake build type for libxml2.",
    "libxml2_build_dir": "Directory where libxml2 will be built.",
    "libxml2_install_dir": "Directory where libxml2 will be installed.",
    "libxml2_repo": "URL of the libxml2 repository to clone.",
    "libxml2_branch": "Branch or tag of libxml2 to use.",
    "llvm_src_dir": "Directory for the LLVM project source code.",
    "llvm_build_type": "CMake build type for LLVM.",
    "llvm_build_dir": "Directory where LLVM will be built.",
    "llvm_install_dir": "Directory where LLVM will be installed.",
    "llvm_repo": "URL of the LLVM project repository to clone.",
    "llvm_commit": "Specific commit hash of LLVM to checkout.",
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
        inp = input(f"{prompt} ({default}): ")
        result = inp.strip() or default
        return result

    def prompt_boolean(self, prompt, default = None):
        """
        Prompts the user for a boolean value (yes/no).

        :param prompt: The prompt message to display.
        :param default: The default value to return if the user does not provide input.
        :return: bool: True if the user answers yes, False otherwise.
        """
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
                    fmt = match.group(2)
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
                    if fmt:
                        if fmt == "lower":
                            val = val.lower()
                        elif fmt == "upper":
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
        def supports_ansi():
            if os.name == "posix":
                return True
            if os.name == "nt":
                return "WT_SESSION" in os.environ or sys.stdout.isatty()
            return False

        if cwd is None:
            cwd = os.getcwd()
        color = BLUE if supports_ansi() else ""
        reset = RESET if supports_ansi() else ""
        if isinstance(cmd, list):
            print(f"{color}{cwd}> {' '.join(cmd)}{reset}")
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
            config_args.extend(["-B",  build_dir])
        if build_type:
            config_args.extend([f"-DCMAKE_BUILD_TYPE={cmake_build_type}"])
            if build_type_is_debwithopt:
                if self.is_windows():
                    config_args.extend(["-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /Ob1 /O2 /Zi", "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"])
                    config_args.extend(["-DCMAKE_CXX_FLAGS=/Ob1 /O2 /Zi", "-DCMAKE_C_FLAGS=/Ob1 /O2 /Zi"])
                else:
                    config_args.extend(["-DCMAKE_CXX_FLAGS=-Og -g", "-DCMAKE_C_FLAGS=-Og -g"])
                    config_args.extend(["-DCMAKE_CXX_FLAGS_DEBUG=-Og -g", "-DCMAKE_C_FLAGS_DEBUG=-Og -g"])
        if isinstance(extra_args, str):
            config_args.extend(extra_args.split())
        elif isinstance(extra_args, list):
            config_args.extend(extra_args)
        self.run_cmd(config_args)

        build_args = [self.options.cmake_path, "--build", build_dir, "--config", cmake_build_type]
        num_cores = os.cpu_count() or 1
        max_safe_parallel = 4 # Ideally 4GB per job
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
            if not self.prompt_boolean(f"Source directory '{self.options.mrdocs_src_dir}' does not exist. Create and clone MrDocs there?", True):
                print("Installation aborted by user.")
                return
            self.prompt_option("mrdocs_branch")
            self.prompt_option("mrdocs_repo")
            self.clone_repo(self.options.mrdocs_repo, self.options.mrdocs_src_dir, branch=self.options.mrdocs_branch)
        else:
            if not os.path.isdir(self.options.mrdocs_src_dir):
                raise NotADirectoryError(f"Specified mrdocs_src_dir '{self.options.mrdocs_src_dir}' is not a directory.")

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

    def install_fmt(self):
        self.prompt_dependency_path_option("fmt_src_dir")
        if not os.path.exists(self.options.fmt_src_dir):
            self.prompt_option("fmt_repo")
            self.prompt_option("fmt_branch")
            self.clone_repo(self.options.fmt_repo, self.options.fmt_src_dir, branch=self.options.fmt_branch, depth=1)
        self.prompt_build_type_option("fmt_build_type")
        self.prompt_dependency_path_option("fmt_build_dir")
        self.prompt_dependency_path_option("fmt_install_dir")
        self.cmake_workflow(self.options.fmt_src_dir, self.options.fmt_build_type, self.options.fmt_build_dir, self.options.fmt_install_dir, ["-D", "FMT_DOC=OFF", "-D", "FMT_TEST=OFF"])

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
        self.cmake_workflow(self.options.duktape_src_dir, self.options.duktape_build_type, self.options.duktape_build_dir, self.options.duktape_install_dir)

    def install_libxml2(self):
        self.prompt_dependency_path_option("libxml2_src_dir")
        if not os.path.exists(self.options.libxml2_src_dir):
            self.prompt_option("libxml2_repo")
            self.prompt_option("libxml2_branch")
            self.clone_repo(self.options.libxml2_repo, self.options.libxml2_src_dir, branch=self.options.libxml2_branch, depth=1)
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
        self.cmake_workflow(self.options.libxml2_src_dir, self.options.libxml2_build_type, self.options.libxml2_build_dir, self.options.libxml2_install_dir, extra_args)

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
        self.cmake_workflow(llvm_subproject_dir, self.options.llvm_build_type, self.options.llvm_build_dir, self.options.llvm_install_dir, cmake_extra_args)

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
                "fmt_ROOT": self.options.fmt_install_dir,
                "libxml2_ROOT": self.options.libxml2_install_dir,
                "LibXml2_ROOT": self.options.libxml2_install_dir,
                "MRDOCS_BUILD_TESTS": self.options.mrdocs_build_tests,
                "MRDOCS_BUILD_DOCS": False,
                "MRDOCS_GENERATE_REFERENCE": False,
                "MRDOCS_GENERATE_ANTORA_REFERENCE": False
            },
            "condition": {
                "type": "equals",
                "lhs": "${hostSystemName}",
                "rhs": hostSystemName
            }
        }

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
            self.options.mrdocs_build_dir = os.path.join(self.options.mrdocs_src_dir, "build", self.options.mrdocs_preset_name)

        if not self.prompt_option("mrdocs_system_install"):
            # Build directory specified in the preset
            # self.prompt_option("mrdocs_build_dir")
            self.prompt_option("mrdocs_install_dir")

        extra_args = []
        if self.options.mrdocs_system_install and self.options.mrdocs_install_dir:
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
                "-D", f"fmt_ROOT={self.options.fmt_install_dir}"
            ])
            if self.options.mrdocs_build_tests:
                extra_args.extend([
                    "-D", f"libxml2_ROOT={self.options.libxml2_install_dir}",
                    "-D", f"LibXml2_ROOT={self.options.libxml2_install_dir}"
                ])
                extra_args.extend(["-D", "MRDOCS_BUILD_TESTS=ON"])
            extra_args.extend(["-DMRDOCS_BUILD_DOCS=OFF", "-DMRDOCS_GENERATE_REFERENCE=OFF", "-DMRDOCS_GENERATE_ANTORA_REFERENCE=OFF"])

        self.cmake_workflow(self.options.mrdocs_src_dir, self.options.mrdocs_build_type, self.options.mrdocs_build_dir, self.options.mrdocs_install_dir, extra_args)
        if self.options.mrdocs_build_dir and self.prompt_option("mrdocs_run_tests"):
            # Look for ctest path relative to the cmake path
            ctest_path = os.path.join(os.path.dirname(self.options.cmake_path), "ctest")
            if not os.path.exists(ctest_path):
                raise FileNotFoundError(f"ctest executable not found at {ctest_path}. Please ensure CMake is installed correctly.")
            # --parallel 4 --no-tests=error --progress --output-on-failure
            test_args = [ctest_path, "--test-dir", self.options.mrdocs_build_dir, "--output-on-failure", "--progress", "--no-tests=error", "--output-on-failure"]
            self.run_cmd(test_args)

        print(f"\nMrDocs has been successfully installed in {self.options.mrdocs_install_dir}.\n")

    def install_all(self):
        self.check_tools()
        self.setup_mrdocs_dir()
        self.setup_third_party_dir()
        self.install_fmt()
        self.install_duktape()
        if self.options.mrdocs_build_tests:
            self.install_libxml2()
        self.install_llvm()
        self.create_cmake_presets()
        self.install_mrdocs()

def get_command_line_args():
    """
    Parses command line arguments and returns them as a dictionary.

    Every field in the InstallOptions dataclass is converted to a
    valid command line argument description.

    :return: dict: Dictionary of command line arguments.
    """
    parser = argparse.ArgumentParser(description="Download and install MrDocs from source.")
    for field in dataclasses.fields(InstallOptions):
        arg_name = f"--{field.name.replace('_', '-')}"
        help_text = INSTALL_OPTION_DESCRIPTIONS.get(field.name)
        if help_text is None:
            raise ValueError(f"Missing description for option '{field.name}' in INSTALL_OPTION_DESCRIPTIONS.")
        help_text += f" (default: {field.default})" if (field.default is not dataclasses.MISSING and field.default is not None) else ""
        if field.type is bool:
            parser.add_argument(arg_name, action="store_const", const=True, default=None, help=help_text)
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
