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
import platform
import shutil
from dataclasses import dataclass, field
import dataclasses
import urllib.request
import tarfile
import json
import shlex
import re
import zipfile
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
    # Compiler
    cc: str = ''
    cxx: str = ''
    sanitizer: str = ''

    # Required tools
    git_path: str = ''
    cmake_path: str = ''
    python_path: str = ''

    # Test tools
    java_path: str = ''

    # Optional tools
    ninja_path: str = ''

    # MrDocs
    mrdocs_src_dir: str = field(
        default_factory=lambda: os.getcwd() if running_from_mrdocs_source_dir() else os.path.join(os.getcwd(),
                                                                                                  "mrdocs"))
    mrdocs_build_type: str = "Release"
    mrdocs_repo: str = "https://github.com/cppalliance/mrdocs"
    mrdocs_branch: str = "develop"
    mrdocs_use_user_presets: bool = True
    mrdocs_preset_name: str = "<mrdocs-build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    mrdocs_build_dir: str = "<mrdocs-src-dir>/build/<mrdocs-build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower><\"-\":if(sanitizer)><sanitizer:lower>"
    mrdocs_build_tests: bool = True
    mrdocs_system_install: bool = field(default_factory=lambda: not running_from_mrdocs_source_dir())
    mrdocs_install_dir: str = field(
        default_factory=lambda: "<mrdocs-src-dir>/install/<mrdocs-build-type:lower>-<os:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>" if running_from_mrdocs_source_dir() else "")
    mrdocs_run_tests: bool = True

    # Third-party dependencies
    third_party_src_dir: str = "<mrdocs-src-dir>/build/third-party"

    # Duktape
    duktape_src_dir: str = "<third-party-src-dir>/duktape"
    duktape_url: str = "https://github.com/svaarala/duktape/releases/download/v2.7.0/duktape-2.7.0.tar.xz"
    duktape_build_type: str = "<mrdocs-build-type>"
    duktape_build_dir: str = "<duktape-src-dir>/build/<duktape-build-type:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    duktape_install_dir: str = "<duktape-src-dir>/install/<duktape-build-type:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"

    # LLVM
    llvm_src_dir: str = "<third-party-src-dir>/llvm-project"
    llvm_build_type: str = "<mrdocs-build-type>"
    llvm_build_dir: str = "<llvm-src-dir>/build/<llvm-build-type:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    llvm_install_dir: str = "<llvm-src-dir>/install/<llvm-build-type:lower><\"-\":if(cc)><cc:basename><\"-\":if(sanitizer)><sanitizer:lower>"
    llvm_repo: str = "https://github.com/llvm/llvm-project.git"
    llvm_commit: str = "dd7a3d4d798e30dfe53b5bbbbcd9a23c24ea1af9"

    # Libxml2
    libxml2_src_dir: str = "<third-party-src-dir>/libxml2"
    libxml2_build_type: str = "Release" # purposefully does not depend on mrdocs-build-type because we only need the executable
    libxml2_build_dir: str = "<libxml2-src-dir>/build/<libxml2-build-type:lower><\"-\":if(cc)><cc:basename>"
    libxml2_install_dir: str = "<libxml2-src-dir>/install/<libxml2-build-type:lower><\"-\":if(cc)><cc:basename>"
    libxml2_repo: str = "https://github.com/GNOME/libxml2"
    libxml2_branch: str = "v2.12.6"

    # Information to create run configurations
    generate_run_configs: bool = field(default_factory=lambda: running_from_mrdocs_source_dir())
    jetbrains_run_config_dir: str = "<mrdocs-src-dir>/.run"
    boost_src_dir: str = "<mrdocs-src-dir>/../boost"

    # Command line arguments
    non_interactive: bool = False
    refresh_all: bool = False


# Constant for option descriptions
INSTALL_OPTION_DESCRIPTIONS = {
    "cc": "Path to the C compiler executable. Leave empty for default.",
    "cxx": "Path to the C++ compiler executable. Leave empty for default.",
    "sanitizer": "Sanitizer to use for the build. Leave empty for no sanitizer. (ASan, UBSan, MSan, TSan)",
    "git_path": "Path to the git executable, if not in system PATH.",
    "cmake_path": "Path to the cmake executable, if not in system PATH.",
    "python_path": "Path to the python executable, if not in system PATH.",
    "java_path": "Path to the java executable, if not in system PATH.",
    "ninja_path": "Path to the ninja executable. Leave empty to download it automatically.",
    "mrdocs_src_dir": "MrDocs source directory.",
    "mrdocs_repo": "URL of the MrDocs repository to clone.",
    "mrdocs_branch": "Branch or tag of the MrDocs repository to use.",
    "mrdocs_build_type": "CMake build type for MrDocs (Release, Debug, RelWithDebInfo, MilRelSize).",
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
    "duktape_build_type": "CMake build type for Duktape. (Release, Debug, RelWithDebInfo, MilRelSize)",
    "duktape_build_dir": "Directory where Duktape will be built.",
    "duktape_install_dir": "Directory where Duktape will be installed.",
    "libxml2_src_dir": "Directory for the libxml2 source code.",
    "libxml2_build_type": "CMake build type for libxml2: Release always recommended. (Release, Debug, RelWithDebInfo, MilRelSize)",
    "libxml2_build_dir": "Directory where libxml2 will be built.",
    "libxml2_install_dir": "Directory where libxml2 will be installed.",
    "libxml2_repo": "URL of the libxml2 repository to clone.",
    "libxml2_branch": "Branch or tag of libxml2 to use.",
    "llvm_src_dir": "Directory for the LLVM project source code.",
    "llvm_build_type": "CMake build type for LLVM. (Release, Debug, RelWithDebInfo, MilRelSize)",
    "llvm_build_dir": "Directory where LLVM will be built.",
    "llvm_install_dir": "Directory where LLVM will be installed.",
    "llvm_repo": "URL of the LLVM project repository to clone.",
    "llvm_commit": "Specific commit hash of LLVM to checkout.",
    "generate_run_configs": "Whether to generate run configurations for IDEs.",
    "jetbrains_run_config_dir": "Directory where JetBrains run configurations will be stored.",
    "boost_src_dir": "Directory where the source files of the Boost libraries are located.",
    "non_interactive": "Whether to use all default options without interactive prompts.",
    "refresh_all": "Call the command to refresh dependencies for all configurations"
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
        self.options.refresh_all = self.cmd_line_args.get("refresh_all", False)
        self.prompted_options = set()
        self.compiler_info = {}

    def prompt_string(self, prompt, default):
        """
        Prompts the user for a string input with a default value.

        :param prompt: The prompt message to display to the user.
        :param default: The default value to use if the user does not provide input.
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
        if default:
            prompt += f" ({default})"
        prompt += f": "
        inp = input(prompt)
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
                    transform_fn = match.group(2)
                    has_dir_key = has_dir_key or key.endswith("-dir")
                    key_surrounded_by_quotes = key.startswith('"') and key.endswith('"')
                    if key_surrounded_by_quotes:
                        val = key[1:-1]
                    else:
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
                            key = key.replace("-", "_")
                            val = getattr(self.options, key, None)

                    if transform_fn:
                        if transform_fn == "lower":
                            val = val.lower()
                        elif transform_fn == "upper":
                            val = val.upper()
                        elif transform_fn == "basename":
                            val = os.path.basename(val)
                        elif transform_fn.startswith("if(") and transform_fn.endswith(")"):
                            var_name = transform_fn[3:-1]
                            if getattr(self.options, var_name, None):
                                val = val.lower()
                            else:
                                val = ""
                    return val

                # Regex: <key> or <key:format>
                pattern = r"<([\"a-zA-Z0-9_\-]+)(?::([a-zA-Z0-9_\-\(\)]+))?>"
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
        valid_build_types = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]
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

    def prompt_sanitizer_option(self, name):
        value = self.prompt_option(name)
        if not value:
            value = ''
            return value
        valid_sanitizers = ["ASan", "UBSan", "MSan", "TSan"]
        for t in valid_sanitizers:
            if t.lower() == value.lower():
                setattr(self.options, name, t)
                return value
        print(f"Invalid sanitizer '{value}'. Must be one of: {', '.join(valid_sanitizers)}.")
        value = self.reprompt_option(name)
        for t in valid_sanitizers:
            if t.lower() == value.lower():
                setattr(self.options, name, t)
                return value
        print(f"Invalid sanitizer '{value}'. Must be one of: {', '.join(valid_sanitizers)}.")
        raise ValueError(f"Invalid sanitizer '{value}'. Must be one of: {', '.join(valid_sanitizers)}.")

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

    def cmake_workflow(self, src_dir, build_type, build_dir, install_dir, extra_args=None, cc_flags=None, cxx_flags=None):
        """
        Configures and builds a CMake project.
        """

        # Adjust any potential CMake flags from extra_args
        if cc_flags is None:
            cc_flags = ""
        if cxx_flags is None:
            cxx_flags = ""
        extra_args_remove_idx = []
        for i in range(0, len(extra_args or [])):
            extra_arg = extra_args[i]
            if extra_arg.startswith('-DCMAKE_C_FLAGS='):
                cc_flags += ' ' + extra_arg.split('=', 1)[1]
                extra_args_remove_idx.append(i)
            elif extra_arg.startswith('-DCMAKE_CXX_FLAGS='):
                cxx_flags += ' ' + extra_arg.split('=', 1)[1]
                extra_args_remove_idx.append(i)
            elif i != 0 and extra_args[i-1].strip() == '-D':
                if extra_arg.startswith('CMAKE_C_FLAGS='):
                    cc_flags += ' ' + extra_arg.split('=', 1)[1]
                    extra_args_remove_idx.append(i - 1)
                    extra_args_remove_idx.append(i)
                elif extra_arg.startswith('CMAKE_CXX_FLAGS='):
                    cxx_flags += ' ' + extra_arg.split('=', 1)[1]
                    extra_args_remove_idx.append(i - 1)
                    extra_args_remove_idx.append(i)
        if extra_args_remove_idx:
            extra_args = [arg for i, arg in enumerate(extra_args or []) if i not in extra_args_remove_idx]

        config_args = [self.options.cmake_path, "-S", src_dir]

        if build_dir:
            config_args.extend(["-B", build_dir])
        if self.options.ninja_path:
            config_args.extend(["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={self.options.ninja_path}"])

        if self.options.cc and self.options.cxx:
            config_args.extend(["-DCMAKE_C_COMPILER=" + self.options.cc,
                                "-DCMAKE_CXX_COMPILER=" + self.options.cxx])

        # If the cmake script happens to look for the python executable, we
        # already provide it on windows because it's often not in PATH.
        if self.is_windows() and self.options.python_path:
            config_args.extend(["-DPYTHON_EXECUTABLE=" + self.options.python_path])

        # "OptimizedDebug" is not a valid build type. We interpret it as a special case
        # where the build type is Debug and optimizations are enabled.
        # This is equivalent to RelWithDebInfo on Unix, but ensures
        # Debug flags and the Debug ABI are used on Windows.
        build_type_is_optimizeddebug = build_type.lower() == 'optimizeddebug'
        cmake_build_type = build_type if not build_type_is_optimizeddebug else "Debug"
        if build_type:
            config_args.extend([f"-DCMAKE_BUILD_TYPE={cmake_build_type}"])
            if build_type_is_optimizeddebug:
                if self.is_windows():
                    cxx_flags += " /DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"
                    cc_flags += " /DWIN32 /D_WINDOWS /Ob1 /O2 /Zi"
                else:
                    cxx_flags += " -Og -g"
                    cc_flags += " -Og -g"

        if isinstance(extra_args, list):
            config_args.extend(extra_args)
        else:
            raise TypeError(f"extra_args must be a list, got {type(extra_args)}.")

        if self.is_homebrew_clang():
            homebrew_clang_root = os.path.dirname(os.path.dirname(self.options.cxx))
            ar_path = os.path.join(homebrew_clang_root, "bin", "llvm-ar")
            if self.is_executable(ar_path):
                for cxx_flag_var in [
                    "CMAKE_AR",
                    "CMAKE_CXX_COMPILER_AR",
                    "CMAKE_C_COMPILER_AR"
                ]:
                    config_args.append(f"-D{cxx_flag_var}={ar_path}")
            runlib_path = os.path.join(homebrew_clang_root, "bin", "llvm-ranlib")
            if self.is_executable(runlib_path):
                config_args.append(f"-DCMAKE_RANLIB={runlib_path}")
            ld_path = os.path.join(homebrew_clang_root, "bin", "ld.lld")
            if self.is_executable(ld_path):
                config_args.append(f"-DCMAKE_C_COMPILER_LINKER={ld_path}")
                config_args.append(f"-DCMAKE_CXX_COMPILER_LINKER={ld_path}")
            else:
                ld_path = '/opt/homebrew/bin/ld.lld'
                if self.is_executable(ld_path):
                    config_args.append(f"-DCMAKE_C_COMPILER_LINKER={ld_path}")
                    config_args.append(f"-DCMAKE_CXX_COMPILER_LINKER={ld_path}")
            libcxx_include = os.path.join(homebrew_clang_root, "include", "c++", "v1")
            libcxx_lib = os.path.join(homebrew_clang_root, "lib", "c++")
            libunwind = os.path.join(homebrew_clang_root, "lib", "unwind")
            if os.path.exists(libcxx_include) and os.path.exists(libcxx_lib) and os.path.exists(libunwind):
                cxx_flags += f' -stdlib=libc++ -I{libcxx_include}'
                ld_flags = f'-L{libcxx_lib} -L{libunwind} -lunwind'
                if self.options.sanitizer:
                    ld_flags += f' -fsanitize={self.sanitizer_flag_name(self.options.sanitizer)}'
                for cxx_linker_flag_var in [
                    "CMAKE_EXE_LINKER_FLAGS",
                    "CMAKE_SHARED_LINKER_FLAGS",
                    "CMAKE_MODULE_LINKER_FLAGS"
                ]:
                    config_args.append(f"-D{cxx_linker_flag_var}={ld_flags}")

        if cc_flags:
            config_args.append(f"-DCMAKE_C_FLAGS={cc_flags.strip()}")
        if cxx_flags:
            config_args.append(f"-DCMAKE_CXX_FLAGS={cxx_flags.strip()}")
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
        if not os.path.exists(path):
            return False
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

    def check_compilers(self):
        for option in ["cc", "cxx"]:
            self.prompt_option(option)
            if getattr(self.options, option):
                if not os.path.isabs(getattr(self.options, option)):
                    exec = shutil.which(getattr(self.options, option))
                    if exec is None:
                        raise FileNotFoundError(
                            f"{option} executable '{getattr(self.options, option)}' not found in PATH.")
                    setattr(self.options, option, exec)
                if not self.is_executable(getattr(self.options, option)):
                    raise FileNotFoundError(f"{option} executable not found at {getattr(self.options, option)}.")

    def check_tools(self):
        tools = ["git", "cmake", "python"]
        for tool in tools:
            self.check_tool(tool)

    def setup_mrdocs_src_dir(self):
        self.prompt_option("mrdocs_src_dir")
        if not os.path.isabs(self.options.mrdocs_src_dir):
            self.options.mrdocs_src_dir = os.path.abspath(self.options.mrdocs_src_dir)
        if not os.path.exists(self.options.mrdocs_src_dir):
            if not self.prompt_boolean(
                    f"Source directory '{self.options.mrdocs_src_dir}' does not exist. Create and clone MrDocs there?",
                    True):
                print("Installation aborted by user.")
                exit(1)
            self.prompt_option("mrdocs_branch")
            self.prompt_option("mrdocs_repo")
            self.clone_repo(self.options.mrdocs_repo, self.options.mrdocs_src_dir, branch=self.options.mrdocs_branch)
        else:
            if not os.path.isdir(self.options.mrdocs_src_dir):
                raise NotADirectoryError(
                    f"Specified mrdocs_src_dir '{self.options.mrdocs_src_dir}' is not a directory.")

        # MrDocs build type
        self.prompt_build_type_option("mrdocs_build_type")
        self.prompt_sanitizer_option("sanitizer")
        if self.prompt_option("mrdocs_build_tests"):
            self.check_tool("java")

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
        if not os.path.exists(value):
            if not self.prompt_boolean(f"'{value}' does not exist. Create it?", True):
                raise FileNotFoundError(f"'{value}' does not exist and user chose not to create it.")
        return value

    def setup_third_party_dir(self):
        self.prompt_dependency_path_option("third_party_src_dir")
        os.makedirs(self.options.third_party_src_dir, exist_ok=True)

    @lru_cache(maxsize=1)
    def probe_compilers(self):
        variables = []
        for lang in ["C", "CXX"]:
            for suffix in ["COMPILER", "COMPILER_ID", "COMPILER_VERSION", "COMPILER_AR", "COMPILER_LINKER",
                           "COMPILER_LINKER_ID", "COMPILER_ABI"]:
                variables.append(f"CMAKE_{lang}_{suffix}")
        variables.append("CMAKE_GENERATOR")

        probe_dir = os.path.join(self.options.third_party_src_dir, "cmake-probe")
        if os.path.exists(probe_dir):
            shutil.rmtree(probe_dir)
        os.makedirs(probe_dir, exist_ok=True)

        # Create minimal CMakeLists.txt
        cmake_lists = [
            "cmake_minimum_required(VERSION 3.10)",
            "project(probe C CXX)"
        ]
        for var in variables:
            cmake_lists.append(f'message(STATUS "{var}=${{{var}}}")')
        with open(os.path.join(probe_dir, "CMakeLists.txt"), "w") as f:
            f.write("\n".join(cmake_lists))

        # Build command
        cmd = ["cmake", "-S", probe_dir]
        env = os.environ.copy()
        if self.options.cc:
            cmd += ["-DCMAKE_C_COMPILER=" + self.options.cc]
        if self.options.cxx:
            cmd += ["-DCMAKE_CXX_COMPILER=" + self.options.cxx]
        cmd += ["-B", os.path.join(probe_dir, "build")]

        # Run cmake and capture output
        result = subprocess.run(cmd, env=env, text=True, capture_output=True)
        if result.returncode != 0:
            raise RuntimeError(f"CMake failed:\n{result.stdout}\n{result.stderr}")

        # Parse values from lines like: "-- VAR=value"
        values = {}
        for line in result.stdout.splitlines():
            if line.startswith("-- "):
                for var in variables:
                    prefix = f"{var}="
                    if prefix in line:
                        values[var] = line.split(prefix, 1)[1].strip()

        # Store this map in self for later use
        self.compiler_info = values

        # Clean up probe directory
        shutil.rmtree(probe_dir)

    @lru_cache(maxsize=1)
    def is_homebrew_clang(self):
        self.probe_compilers()
        if not self.is_macos():
            return False
        if not self.compiler_info:
            return False
        if self.compiler_info["CMAKE_CXX_COMPILER_ID"].lower() != "clang":
            return False
        out = subprocess.run([self.options.cxx, "--version"], capture_output=True, text=True)
        version = out.stdout.strip()
        return "Homebrew clang" in version

    def install_ninja(self):
        # 1. Check if the user has set a ninja_path option
        if self.prompt_option("ninja_path"):
            if not os.path.isabs(self.options.ninja_path):
                self.options.ninja_path = shutil.which(self.options.ninja_path)
            if not self.is_executable(self.options.ninja_path):
                raise FileNotFoundError(f"Ninja executable not found at {self.options.ninja_path}.")
            return

        # 2. If ninja_path is not set, but does the user have it available in PATH?
        ninja_path = shutil.which("ninja")
        if ninja_path:
            print(f"Ninja found in PATH at {ninja_path}. Using it.")
            self.options.ninja_path = ninja_path
            return

        # 3. Ninja path isn't set and not available in PATH, so we download it
        destination_dir = self.options.third_party_src_dir
        ninja_dir = os.path.join(destination_dir, "ninja")
        exe_name = 'ninja.exe' if platform.system().lower() == 'windows' else 'ninja'
        ninja_path = os.path.join(ninja_dir, exe_name)
        if os.path.exists(ninja_path) and self.is_executable(ninja_path):
            print(f"Ninja already exists at {ninja_path}. Using it.")
            self.options.ninja_path = ninja_path
            return

        # 3a. Determine the ninja asset name based on the platform and architecture
        system = platform.system().lower()
        arch = platform.machine().lower()
        if system == 'linux':
            if arch in ('aarch64', 'arm64'):
                asset_name = 'ninja-linux-aarch64.zip'
            else:
                asset_name = 'ninja-linux.zip'
        elif system == 'darwin':
            asset_name = 'ninja-mac.zip'
        elif system == 'windows':
            if arch in ('arm64', 'aarch64'):
                asset_name = 'ninja-winarm64.zip'
            else:
                asset_name = 'ninja-win.zip'
        else:
            return

        # 3b. Find the download URL for the latest Ninja release asset
        api_url = 'https://api.github.com/repos/ninja-build/ninja/releases/latest'
        with urllib.request.urlopen(api_url) as resp:
            data = json.load(resp)
        release_assets = data.get('assets', [])
        download_url = None
        for a in release_assets:
            if a.get('name') == asset_name:
                download_url = a.get('browser_download_url')
                break
        if not download_url:
            # Could not find release asset named asset_name
            return

        # 3c. Download the asset to the third-party source directory
        tmpzip = os.path.join(destination_dir, asset_name)
        os.makedirs(destination_dir, exist_ok=True)
        print(f'Downloading {asset_name} …')
        urllib.request.urlretrieve(download_url, tmpzip)

        # 3d. Extract the downloaded zip file into the ninja dir
        print('Extracting…')
        os.makedirs(ninja_dir, exist_ok=True)
        with zipfile.ZipFile(tmpzip, 'r') as z:
            z.extractall(ninja_dir)
        os.remove(tmpzip)

        # 3e. Set the ninja_path option to the extracted ninja executable
        if platform.system().lower() != 'windows':
            os.chmod(ninja_path, 0o755)
        self.options.ninja_path = ninja_path

    def sanitizer_flag_name(self, sanitizer):
        """
        Returns the flag name for the given sanitizer.
        :param sanitizer: The sanitizer name (ASan, UBSan, MSan, TSan).
        :return: str: The flag name for the sanitizer.
        """
        if sanitizer.lower() == "asan":
            return "address"
        elif sanitizer.lower() == "ubsan":
            return "undefined"
        elif sanitizer.lower() == "msan":
            return "memory"
        elif sanitizer.lower() == "tsan":
            return "thread"
        else:
            raise ValueError(f"Unknown sanitizer '{sanitizer}'.")

    def is_abi_compatible(self, build_type_a, build_type_b):
        if not self.is_windows():
            return True
        # On Windows, Debug and Release builds are not ABI compatible
        build_type_a_is_debug = build_type_a.lower() == "debug"
        build_type_b_is_debug = build_type_b.lower() == "debug"
        return build_type_a_is_debug == build_type_b_is_debug

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
            os.remove(archive_path)
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
        if not self.is_abi_compatible(self.options.mrdocs_build_type, self.options.duktape_build_type):
            if self.options.mrdocs_build_type.lower() == "debug":
                # User asked for Release dependency, so we do the best we can and change it to
                # an optimized debug build.
                self.options.duktape_build_type = "OptimizedDebug"
            else:
                # User asked for a Debug dependency with Release build type for MrDocs.
                # The dependency should just copy the release type here. Other options wouldn't make sense
                # because we can't even debug it.
                self.options.duktape_build_type = self.options.mrdocs_build_type
        self.prompt_dependency_path_option("duktape_build_dir")
        self.prompt_dependency_path_option("duktape_install_dir")
        extra_args = []
        if self.options.sanitizer:
            flag_name = self.sanitizer_flag_name(self.options.sanitizer)
            for arg in ["CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS"]:
                extra_args.append(
                    f"-D{arg}=-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer")

        self.cmake_workflow(
            self.options.duktape_src_dir,
            self.options.duktape_build_type,
            self.options.duktape_build_dir,
            self.options.duktape_install_dir,
            extra_args)

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
        # Same logic as for Duktape
        if not self.is_abi_compatible(self.options.mrdocs_build_type, self.options.llvm_build_type):
            if self.options.mrdocs_build_type.lower() == "debug":
                self.options.llvm_build_type = "OptimizedDebug"
            else:
                self.options.llvm_build_type = self.options.mrdocs_build_type
        self.prompt_dependency_path_option("llvm_build_dir")
        self.prompt_dependency_path_option("llvm_install_dir")
        cmake_preset = f"{self.options.llvm_build_type.lower()}-win" if self.is_windows() else f"{self.options.llvm_build_type.lower()}-unix"
        cmake_extra_args = [f"--preset={cmake_preset}"]
        if self.options.sanitizer:
            if self.options.sanitizer.lower() == "asan":
                cmake_extra_args.append("-DLLVM_USE_SANITIZER=Address")
            elif self.options.sanitizer.lower() == "ubsan":
                cmake_extra_args.append("-DLLVM_USE_SANITIZER=Undefined")
            elif self.options.sanitizer.lower() == "msan":
                cmake_extra_args.append("-DLLVM_USE_SANITIZER=Memory")
            elif self.options.sanitizer.lower() == "tsan":
                cmake_extra_args.append("-DLLVM_USE_SANITIZER=Thread")
            else:
                raise ValueError(f"Unknown LLVM sanitizer '{self.options.sanitizer}'.")
        self.cmake_workflow(
            llvm_subproject_dir,
            self.options.llvm_build_type,
            self.options.llvm_build_dir,
            self.options.llvm_install_dir,
            cmake_extra_args)

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
        is_debug_fast = self.options.mrdocs_build_type.lower() == "debug" and self.options.llvm_build_type != self.options.mrdocs_build_type
        if is_debug_fast:
            self.default_options.mrdocs_preset_name += "-fast"
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

        # Preset inherits from the parent preset based on the build type
        parent_preset_name = "debug"
        if self.options.mrdocs_build_type.lower() != "debug":
            parent_preset_name = "release"
            if self.options.mrdocs_build_type.lower() == "relwithdebinfo":
                parent_preset_name = "relwithdebinfo"

        # Nice display name for the preset
        display_name = f"{self.options.mrdocs_build_type}"
        if self.options.mrdocs_build_type.lower() == "debug" and self.options.llvm_build_type != self.options.mrdocs_build_type:
            display_name += " with Optimized Dependencies"
        display_name += f" ({OSDisplayName}"
        if self.options.cc:
            display_name += f": {os.path.basename(self.options.cc)}"
        display_name += ")"

        if self.options.sanitizer:
            display_name += f" with {self.options.sanitizer}"

        generator = "Unix Makefiles" if not self.is_windows() else "Visual Studio 17 2022"
        if self.options.ninja_path:
            generator = "Ninja"
        elif "CMAKE_GENERATOR" in self.compiler_info:
            generator = self.compiler_info["CMAKE_GENERATOR"]

        new_preset = {
            "name": self.options.mrdocs_preset_name,
            "generator": generator,
            "displayName": display_name,
            "description": f"Preset for building MrDocs in {self.options.mrdocs_build_type} mode with the {os.path.basename(self.options.cc) if self.options.cc else 'default'} compiler in {OSDisplayName}.",
            "inherits": parent_preset_name,
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": self.options.mrdocs_build_type,
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

        if self.options.cc:
            new_preset["cacheVariables"]["CMAKE_C_COMPILER"] = self.options.cc
        if self.options.cxx:
            new_preset["cacheVariables"]["CMAKE_CXX_COMPILER"] = self.options.cxx
        if self.options.ninja_path:
            new_preset["cacheVariables"]["CMAKE_MAKE_PROGRAM"] = self.options.ninja_path

        cc_flags = ''
        cxx_flags = ''
        if self.options.sanitizer:
            flag_name = self.sanitizer_flag_name(self.options.sanitizer)
            cc_flags = f"-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer"
            cxx_flags = f"-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer"

        if self.is_homebrew_clang():
            homebrew_clang_root = os.path.dirname(os.path.dirname(self.options.cxx))
            ar_path = os.path.join(homebrew_clang_root, "bin", "llvm-ar")
            if self.is_executable(ar_path):
                for cxx_flag_var in [
                    "CMAKE_AR",
                    "CMAKE_CXX_COMPILER_AR",
                    "CMAKE_C_COMPILER_AR"
                ]:
                    new_preset["cacheVariables"][cxx_flag_var] = ar_path
            runlib_path = os.path.join(homebrew_clang_root, "bin", "llvm-ranlib")
            if self.is_executable(runlib_path):
                new_preset["cacheVariables"]['CMAKE_RANLIB'] = runlib_path
            ld_path = os.path.join(homebrew_clang_root, "bin", "ld.lld")
            if self.is_executable(ld_path):
                new_preset["cacheVariables"]['CMAKE_C_COMPILER_LINKER'] = ld_path
                new_preset["cacheVariables"]['CMAKE_CXX_COMPILER_LINKER'] = ld_path
            else:
                ld_path = '/opt/homebrew/bin/ld.lld'
                if self.is_executable(ld_path):
                    new_preset["cacheVariables"]['CMAKE_C_COMPILER_LINKER'] = ld_path
                    new_preset["cacheVariables"]['CMAKE_CXX_COMPILER_LINKER'] = ld_path
            libcxx_include = os.path.join(homebrew_clang_root, "include", "c++", "v1")
            libcxx_lib = os.path.join(homebrew_clang_root, "lib", "c++")
            libunwind = os.path.join(homebrew_clang_root, "lib", "unwind")
            if os.path.exists(libcxx_include) and os.path.exists(libcxx_lib) and os.path.exists(libunwind):
                cxx_flags += f' -stdlib=libc++ -I{libcxx_include}'
                ld_flags = f'-L{libcxx_lib} -L{libunwind} -lunwind'
                if self.options.sanitizer:
                    ld_flags += f' -fsanitize={self.sanitizer_flag_name(self.options.sanitizer)}'
                for cxx_linker_flag_var in [
                    "CMAKE_EXE_LINKER_FLAGS",
                    "CMAKE_SHARED_LINKER_FLAGS",
                    "CMAKE_MODULE_LINKER_FLAGS"
                ]:
                    new_preset["cacheVariables"][cxx_linker_flag_var] = ld_flags

        if cc_flags:
            new_preset["cacheVariables"]['CMAKE_C_FLAGS'] = cc_flags.strip()
        if cxx_flags:
            new_preset["cacheVariables"]['CMAKE_CXX_FLAGS'] = cxx_flags.strip()

        if self.is_windows() and self.options.python_path:
            new_preset["cacheVariables"]["PYTHON_EXECUTABLE"] = self.options.python_path

        # Update cache variables path prefixes with their relative equivalents
        mrdocs_src_dir_parent = os.path.dirname(self.options.mrdocs_src_dir)
        if mrdocs_src_dir_parent == self.options.mrdocs_src_dir:
            mrdocs_src_dir_parent = ''
        home_dir = os.path.expanduser("~")
        for key, value in new_preset["cacheVariables"].items():
            if not isinstance(value, str):
                continue
            # Replace mrdocs-src-dir with ${sourceDir}
            if self.options.mrdocs_src_dir and value.startswith(self.options.mrdocs_src_dir):
                new_value = "${sourceDir}" + value[len(self.options.mrdocs_src_dir):]
                new_preset["cacheVariables"][key] = new_value
            # Replace mrdocs-src-dir parent with ${sourceParentDir}
            elif mrdocs_src_dir_parent and value.startswith(mrdocs_src_dir_parent):
                new_value = "${sourceParentDir}" + value[len(mrdocs_src_dir_parent):]
                new_preset["cacheVariables"][key] = new_value
            # Replace $HOME with $env{HOME}
            elif home_dir and value.startswith(home_dir):
                new_value = "$env{HOME}" + value[len(home_dir):]
                new_preset["cacheVariables"][key] = new_value

        # Upsert preset
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
            self.prompt_option("mrdocs_build_dir")
        else:
            self.options.mrdocs_build_dir = os.path.join(self.options.mrdocs_src_dir, "build",
                                                         self.options.mrdocs_preset_name)
            self.default_options.mrdocs_build_dir = self.options.mrdocs_build_dir

        if not self.prompt_option("mrdocs_system_install"):
            if self.options.mrdocs_use_user_presets:
                self.default_options.mrdocs_install_dir = os.path.join(self.options.mrdocs_src_dir, "install",
                                                                       self.options.mrdocs_preset_name)
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

            if self.options.sanitizer:
                flag_name = self.sanitizer_flag_name(self.options.sanitizer)
                for arg in ["CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS"]:
                    extra_args.append(
                        f"-D{arg}=-fsanitize={flag_name} -fno-sanitize-recover={flag_name} -fno-omit-frame-pointer")

        self.cmake_workflow(self.options.mrdocs_src_dir, self.options.mrdocs_build_type, self.options.mrdocs_build_dir,
                            self.options.mrdocs_install_dir, extra_args)

        if self.options.mrdocs_build_dir and self.prompt_option("mrdocs_run_tests"):
            # Look for ctest path relative to the cmake path
            ctest_path = os.path.join(os.path.dirname(self.options.cmake_path), "ctest")
            if self.is_windows():
                ctest_path += ".exe"
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
                attrib = {
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
                    attrib = {
                        "default": "false",
                        "name": config["name"],
                        "type": "ShConfigurationType"
                    }
                    if 'folder' in config:
                        attrib["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrib)
                    ET.SubElement(clion_config, "option", name="SCRIPT_TEXT",
                                  value=f"bash {shlex.quote(config['script'])}")
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
                    ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="false")
                    ET.SubElement(clion_config, "envs")
                    ET.SubElement(clion_config, "method", v="2")
                elif config["script"].endswith(".js"):
                    attrb = {
                        "default": "false",
                        "name": config["name"],
                        "type": "NodeJSConfigurationType",
                        "path-to-js-file": config["script"],
                        "working-dir": config.get("cwd", "$PROJECT_DIR$")
                    }
                    if 'folder' in config:
                        attrb["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrb)
                    envs = ET.SubElement(clion_config, "envs")
                    if 'env' in config:
                        for key, value in config['env'].items():
                            ET.SubElement(envs, "env", name=key, value=value)
                    ET.SubElement(clion_config, "method", v="2")
                elif config["script"] == "npm":
                    attrib = {
                        "default": "false",
                        "name": config["name"],
                        "type": "js.build_tools.npm"
                    }
                    if 'folder' in config:
                        attrib["folderName"] = config["folder"]
                    clion_config = ET.SubElement(root, "configuration", attrib)
                    ET.SubElement(clion_config, "package-json", value=os.path.join(config["cwd"], "package.json"))
                    ET.SubElement(clion_config, "command", value=config["args"][0] if config["args"] else "ci")
                    ET.SubElement(clion_config, "node-interpreter", value="project")
                    envs = ET.SubElement(clion_config, "envs")
                    if 'env' in config:
                        for key, value in config['env'].items():
                            ET.SubElement(envs, "env", name=key, value=value)
                    ET.SubElement(clion_config, "method", v="2")
                else:
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
                                  value=f"{shlex.quote(config['script'])} {' '.join(shlex.quote(arg) for arg in args)}")
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
                    ET.SubElement(clion_config, "option", name="EXECUTE_SCRIPT_FILE", value="false")
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
                "cwd": config.get('cwd', self.options.mrdocs_build_dir),
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

    def generate_vscode_run_configs(self, configs):
        # Visual Studio launch configs are stored in .vs/launch.vs.json
        vscode_dir = os.path.join(self.options.mrdocs_src_dir, ".vscode")
        os.makedirs(vscode_dir, exist_ok=True)
        launch_path = os.path.join(vscode_dir, "launch.json")
        tasks_path = os.path.join(vscode_dir, "tasks.json")

        # Load existing configs if present
        if os.path.exists(launch_path):
            with open(launch_path, "r") as f:
                launch_data = json.load(f)
        else:
            launch_data = {"version": "0.2.0", "configurations": []}

        if os.path.exists(tasks_path):
            with open(tasks_path, "r") as f:
                tasks_data = json.load(f)
        else:
            tasks_data = {"version": "2.0.0", "tasks": []}

        # Build a dict for quick lookup by name
        vs_configs_by_name = {cfg.get("name"): cfg for cfg in launch_data.get("configurations", [])}
        vs_tasks_by_name = {task.get("label"): task for task in tasks_data.get("tasks", [])}

        # Replace with config placeholders
        def replace_with_placeholders(new_config):
            for key, value in new_config.items():
                if isinstance(value, str):
                    new_config[key] = value.replace(self.options.mrdocs_src_dir, "${workspaceFolder}")
                elif isinstance(value, list):
                    for i in range(len(value)):
                        if isinstance(value[i], str):
                            value[i] = value[i].replace(self.options.mrdocs_src_dir, "${workspaceFolder}")
                elif isinstance(value, dict):
                    for sub_key, sub_value in value.items():
                        if isinstance(sub_value, str):
                            value[sub_key] = sub_value.replace(self.options.mrdocs_src_dir, "${workspaceFolder}")

        bootstrap_refresh_config_name = self.options.mrdocs_preset_name or self.options.mrdocs_build_type or "debug"
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
                    "args": config["args"],
                    "cwd": config.get('cwd', self.options.mrdocs_build_dir)
                }

                if 'target' in config:
                    # new_cfg["projectTarget"] = config["target"]
                    new_cfg["name"] += f" ({bootstrap_refresh_config_name})"
                    new_cfg["type"] = "cppdbg"
                    if 'program' in config:
                        new_cfg["program"] = config["program"]
                    else:
                        new_cfg["program"] = os.path.join(self.options.mrdocs_build_dir, config["target"])
                    new_cfg["environment"] = []
                    new_cfg["stopAtEntry"] = False
                    new_cfg["externalConsole"] = False
                    new_cfg["preLaunchTask"] = f"CMake Build {config['target']} ({bootstrap_refresh_config_name})"
                    if self.compiler_info["CMAKE_CXX_COMPILER_ID"].lower() != "clang":
                        lldb_path = shutil.which("lldb")
                        if lldb_path:
                            new_cfg["MIMode"] = "lldb"
                        else:
                            clang_path = self.compiler_info["CMAKE_CXX_COMPILER"]
                            if clang_path and os.path.exists(clang_path):
                                lldb_path = os.path.join(os.path.dirname(clang_path), "lldb")
                                if os.path.exists(lldb_path):
                                    new_cfg["MIMode"] = "lldb"
                    elif self.compiler_info["CMAKE_CXX_COMPILER_ID"].lower() == "gcc":
                        gdb_path = shutil.which("gdb")
                        if gdb_path:
                            new_cfg["MIMode"] = "gdb"
                        else:
                            gcc_path = self.compiler_info["CMAKE_CXX_COMPILER"]
                            if gcc_path and os.path.exists(gcc_path):
                                gdb_path = os.path.join(os.path.dirname(gcc_path), "gdb")
                                if os.path.exists(gdb_path):
                                    new_cfg["MIMode"] = "gdb"
                if 'script' in config:
                    new_cfg["program"] = config["script"]
                    # set type
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
                        new_cfg["skipFiles"] = [
                            "<node_internals>/**"
                        ]
                        new_cfg["sourceMaps"] = True
                        new_cfg["env"] = {}
                        for key, value in config.get("env", {}).items():
                            new_cfg["env"][key] = value
                    else:
                        raise ValueError(
                            f"Unsupported script type for configuration '{config['name']}': {config['script']}. "
                            "Only Python (.py) and JavaScript (.js) scripts are supported."
                        )

                # Any property that begins with the value of mrdocs_src_dir is replaced with ${workspaceFolder}
                replace_with_placeholders(new_cfg)

                # Replace or add
                vs_configs_by_name[new_cfg["name"]] = new_cfg
            else:
                # This is a script configuration, we will create a task for it
                new_task = {
                    "label": config["name"],
                    "type": "shell",
                    "command": config["script"],
                    "args": config["args"],
                    "options": {},
                    "problemMatcher": [],
                }
                if 'cwd' in config and config["cwd"] != self.options.mrdocs_src_dir:
                    new_task["options"]["cwd"] = config["cwd"]

                # Any property that begins with the value of mrdocs_src_dir is replaced with ${workspaceFolder}
                replace_with_placeholders(new_task)

                # Replace or add
                vs_tasks_by_name[new_task["label"]] = new_task

        # Create tasks for the cmake config and build steps
        cmake_config_args = [
            "-S", "${workspaceFolder}"
        ]
        if self.options.mrdocs_preset_name:
            cmake_config_args.extend(["--preset", self.options.mrdocs_preset_name])
        else:
            cmake_config_args.extend(["-B", self.options.mrdocs_build_dir])
            if self.options.ninja_path:
                cmake_config_args.extend(["-G", "Ninja"])
        cmake_config_task = {
            "label": f"CMake Configure ({bootstrap_refresh_config_name})",
            "type": "shell",
            "command": "cmake",
            "args": cmake_config_args,
            "options": {
                "cwd": "${workspaceFolder}"
            }
        }
        replace_with_placeholders(cmake_config_task)
        vs_tasks_by_name[cmake_config_task["label"]] = cmake_config_task

        unique_targets = set()
        for config in configs:
            if 'target' in config:
                unique_targets.add(config['target'])
        for target in unique_targets:
            build_args = [
                "--build", self.options.mrdocs_build_dir,
                "--target", target
            ]
            cmake_build_task = {
                "label": f"CMake Build {target} ({bootstrap_refresh_config_name})",
                "type": "shell",
                "command": "cmake",
                "args": build_args,
                "options": {
                    "cwd": "${workspaceFolder}"
                },
                "dependsOn": f"CMake Configure ({bootstrap_refresh_config_name})",
                "dependsOrder": "sequence",
                "group": "build"
            }
            replace_with_placeholders(cmake_build_task)
            vs_tasks_by_name[cmake_build_task["label"]] = cmake_build_task

        # Write back all configs
        launch_data["configurations"] = list(vs_configs_by_name.values())
        with open(launch_path, "w") as f:
            json.dump(launch_data, f, indent=4)

        tasks_data["tasks"] = list(vs_tasks_by_name.values())
        with open(tasks_path, "w") as f:
            json.dump(tasks_data, f, indent=4)

    def generate_run_configs(self):
        # Configurations using MrDocs executable
        configs = [{
            "name": "MrDocs Version",
            "target": "mrdocs",
            "program": os.path.join(self.options.mrdocs_build_dir, "mrdocs"),
            "args": ["--version"]
        }, {
            "name": "MrDocs Help",
            "target": "mrdocs",
            "program": os.path.join(self.options.mrdocs_build_dir, "mrdocs"),
            "args": ["--help"]
        }]

        # Configuration to run unit tests
        if self.options.mrdocs_build_tests:
            configs.append({
                "name": "MrDocs Unit Tests",
                "target": "mrdocs-test",
                "program": os.path.join(self.options.mrdocs_build_dir, "mrdocs-test"),
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
                    "program": os.path.join(self.options.mrdocs_build_dir, "mrdocs-test"),
                    "folder": "MrDocs Test Fixtures",
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

        num_cores = os.cpu_count() or 1
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
                            "program": os.path.join(self.options.mrdocs_build_dir, "mrdocs"),
                            "args": [
                                '../CMakeLists.txt',
                                f'--config={self.options.boost_src_dir}/libs/{lib}/doc/mrdocs.yml',
                                f'--output={self.options.boost_src_dir}/libs/{lib}/doc/modules/reference/pages',
                                f'--generator=adoc',
                                f'--addons={self.options.mrdocs_src_dir}/share/mrdocs/addons',
                                f'--stdlib-includes={self.options.llvm_install_dir}/include/c++/v1',
                                f'--stdlib-includes={self.options.llvm_install_dir}/lib/clang/20/include',
                                f'--libc-includes={self.options.mrdocs_src_dir}/share/mrdocs/headers/libc-stubs',
                                f'--tagfile=reference.tag.xml',
                                '--multipage=true',
                                f'--concurrency={num_cores}',
                                '--log-level=debug'
                            ]
                        })
            else:
                print(
                    f"Warning: Boost source directory '{self.options.boost_src_dir}' does not contain 'libs' directory. Skipping Boost documentation target generation.")

        # Target to generate the documentation for MrDocs itself
        configs.append({
            "name": f"MrDocs Self-Reference",
            "target": "mrdocs",
            "program": os.path.join(self.options.mrdocs_build_dir, "mrdocs"),
            "args": [
                '../CMakeLists.txt',
                f'--config={self.options.mrdocs_src_dir}/docs/mrdocs.yml',
                f'--output={self.options.mrdocs_src_dir}/docs/modules/reference/pages',
                f'--generator=adoc',
                f'--addons={self.options.mrdocs_src_dir}/share/mrdocs/addons',
                f'--stdlib-includes={self.options.llvm_install_dir}/include/c++/v1',
                f'--stdlib-includes={self.options.llvm_install_dir}/lib/clang/20/include',
                f'--libc-includes={self.options.mrdocs_src_dir}/share/mrdocs/headers/libc-stubs',
                f'--tagfile=reference.tag.xml',
                '--multipage=true',
                f'--concurrency={num_cores}',
                '--log-level=debug'
            ],
            "env": {
                "LLVM_ROOT": self.options.llvm_install_dir,
                "Clang_ROOT": self.options.llvm_install_dir,
                "duktape_ROOT": self.options.duktape_install_dir,
                "Duktape_ROOT": self.options.duktape_install_dir,
                "libxml2_ROOT": self.options.libxml2_install_dir,
                "LibXml2_ROOT": self.options.libxml2_install_dir
            }
        })

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
                    if value != '':
                        bootstrap_args.append(f"--{field.name.replace('_', '-')}")
                        bootstrap_args.append(value)
                else:
                    raise TypeError(f"Unsupported type {field.type} for field '{field.name}' in InstallOptions.")
        bootstrap_refresh_config_name = self.options.mrdocs_preset_name or self.options.mrdocs_build_type or "debug"
        configs.append({
            "name": f"MrDocs Bootstrap Update ({bootstrap_refresh_config_name})",
            "script": os.path.join(self.options.mrdocs_src_dir, "bootstrap.py"),
            "folder": "MrDocs Bootstrap Update",
            "args": bootstrap_args,
            "cwd": self.options.mrdocs_src_dir
        })
        bootstrap_refresh_args = bootstrap_args.copy()
        bootstrap_refresh_args.append("--non-interactive")
        configs.append({
            "name": f"MrDocs Bootstrap Refresh ({bootstrap_refresh_config_name})",
            "script": os.path.join(self.options.mrdocs_src_dir, "bootstrap.py"),
            "folder": "MrDocs Bootstrap Refresh",
            "args": bootstrap_refresh_args,
            "cwd": self.options.mrdocs_src_dir
        })
        configs.append({
            "name": f"MrDocs Bootstrap Refresh All",
            "script": os.path.join(self.options.mrdocs_src_dir, "bootstrap.py"),
            "folder": "MrDocs Bootstrap Refresh",
            "args": ["--refresh-all"],
            "cwd": self.options.mrdocs_src_dir
        })

        # Targets for the pre-build steps
        configs.append({
            "name": f"MrDocs Generate Config Info ({bootstrap_refresh_config_name})",
            "script": os.path.join(self.options.mrdocs_src_dir, 'util', 'generate-config-info.py'),
            "folder": "MrDocs Generate Config Info",
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

        # Render landing page
        mrdocs_website_dir = os.path.join(mrdocs_docs_dir, "website")
        configs.append({
            "name": f"MrDocs Render Landing Page ({bootstrap_refresh_config_name})",
            "script": os.path.join(mrdocs_website_dir, "render.js"),
            "folder": "MrDocs Render Landing Page",
            "args": [],
            "cwd": mrdocs_website_dir,
            "env": {
                "NODE_ENV": "production",
                "MRDOCS_ROOT": self.options.mrdocs_install_dir
            }
        })
        configs.append({
            "name": f"MrDocs Clean Install Website Dependencies",
            "script": "npm",
            "args": ["ci"],
            "cwd": mrdocs_website_dir
        })
        configs.append({
            "name": f"MrDocs Install Website Dependencies",
            "script": "npm",
            "args": ["install"],
            "cwd": mrdocs_website_dir
        })

        # XML schema tests
        if self.options.java_path:
            configs.append({
                "name": "MrDocs Generate RelaxNG Schema",
                "script": self.options.java_path,
                "args": [
                    "-jar",
                    os.path.join(self.options.mrdocs_src_dir, "util", "trang.jar"),
                    os.path.join(self.options.mrdocs_src_dir, "mrdocs.rnc"),
                    os.path.join(self.options.mrdocs_build_dir, "mrdocs.rng")
                ],
                "cwd": self.options.mrdocs_src_dir
            })

            libxml2_xmllint_executable = os.path.join(self.options.libxml2_install_dir, "bin", "xmllint")
            xml_sources_dir = os.path.join(self.options.mrdocs_src_dir, "test-files", "golden-tests")

            if self.is_windows():
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
                        os.path.join(self.options.mrdocs_build_dir, "mrdocs.rng")
                    ].extend(xml_sources),
                    "cwd": self.options.mrdocs_src_dir
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
                        "-exec", libxml2_xmllint_executable,
                        "--dropdtd", "--noout",
                        "--relaxng", os.path.join(self.options.mrdocs_build_dir, "mrdocs.rng"),
                        "{}", "+"
                    ],
                    "cwd": self.options.mrdocs_src_dir
                })

        print("Generating CLion run configurations for MrDocs...")
        self.generate_clion_run_configs(configs)
        print("Generating Visual Studio Code run configurations for MrDocs...")
        self.generate_vscode_run_configs(configs)
        print("Generating Visual Studio run configurations for MrDocs...")
        self.generate_visual_studio_run_configs(configs)

    def install_all(self):
        self.check_compilers()
        self.check_tools()
        self.setup_mrdocs_src_dir()
        self.setup_third_party_dir()
        self.probe_compilers()
        self.install_ninja()
        self.install_duktape()
        self.install_llvm()
        if self.prompt_option("mrdocs_build_tests"):
            self.install_libxml2()
        self.create_cmake_presets()
        self.install_mrdocs()
        if self.prompt_option("generate_run_configs"):
            self.generate_run_configs()
        else:
            print("Skipping run configurations generation as per user preference.")

    def refresh_all(self):
        # 1. Read all configurations in .vscode/launch.json
        current_python_interpreter_path = sys.executable
        this_script_path = os.path.abspath(__file__)
        mrdocs_src_dir = os.path.dirname(this_script_path)
        vscode_launch_path = os.path.join(mrdocs_src_dir, ".vscode", "launch.json")
        if not os.path.exists(vscode_launch_path):
            print("No existing Visual Studio Code launch configurations found.")
            return
        with open(vscode_launch_path, "r") as f:
            vscode_launch_data = json.load(f)
        vscode_configs = vscode_launch_data.get("configurations", [])

        # 2. Filter configurations whose name starts with "MrDocs Bootstrap Refresh ("
        bootstrap_refresh_configs = [
            cfg for cfg in vscode_configs if cfg.get("name", "").startswith("MrDocs Bootstrap Refresh (")
        ]
        if not bootstrap_refresh_configs:
            print("No bootstrap refresh configurations found in Visual Studio Code launch configurations.")
            return

        # 3. For each configuration, run this very same bootstrap.py script with the same arguments
        for config in bootstrap_refresh_configs:
            args = [current_python_interpreter_path, this_script_path] + [
                arg.replace("${workspaceFolder}", mrdocs_src_dir) for arg in config.get("args", [])]
            print(f"Running bootstrap refresh with arguments: {args}")
            subprocess.run(args, check=True)

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
    if installer.options.refresh_all:
        installer.refresh_all()
        exit(0)
    installer.install_all()


if __name__ == "__main__":
    main()
