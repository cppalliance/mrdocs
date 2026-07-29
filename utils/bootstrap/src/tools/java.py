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
Java detection utilities (primarily Windows-specific).

Provides functions to find Java installations.
"""

import os
from typing import Optional

from ..core.platform import is_windows


def find_java() -> Optional[str]:
    """
    Find Java executable on Windows.

    Searches in order:
    1. JAVA_HOME environment variable
    2. Windows Registry (64-bit and 32-bit)
    3. Common Program Files locations

    Returns:
        Path to java.exe, or None if not found.
    """
    if not is_windows():
        return None

    # 1. Check JAVA_HOME environment variable
    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        exe = os.path.join(java_home, "bin", "java.exe")
        if os.path.isfile(exe):
            return exe

    # 2. Check Windows Registry (64-bit and 32-bit)
    try:
        import winreg

        def reg_lookup(base, subkey):
            try:
                with winreg.OpenKey(base, subkey) as key:
                    ver, _ = winreg.QueryValueEx(key, "CurrentVersion")
                    key2 = winreg.OpenKey(base, subkey + "\\" + ver)
                    path, _ = winreg.QueryValueEx(key2, "JavaHome")
                    exe = os.path.join(path, "bin", "java.exe")
                    if os.path.isfile(exe):
                        return exe
            except OSError:
                return None

        for hive, sub in [
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\JavaSoft\Java Runtime Environment"),
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Wow6432Node\JavaSoft\Java Runtime Environment")
        ]:
            result = reg_lookup(hive, sub)
            if result:
                return result
    except ImportError:
        # winreg not available (non-Windows)
        pass

    # 3. Check common Program Files locations
    for base in [os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")]:
        if not base:
            continue
        jroot = os.path.join(base, "Java")
        if os.path.isdir(jroot):
            for entry in os.listdir(jroot):
                candidate = os.path.join(jroot, entry, "bin", "java.exe")
                if os.path.isfile(candidate):
                    return candidate

    return None
