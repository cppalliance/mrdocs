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

"""Entry point for the MrDocs Bootstrap tool."""

import runpy
import sys
from pathlib import Path

if __name__ == "__main__":
    # Run src/ as a package
    sys.path.insert(0, str(Path(__file__).parent))
    runpy.run_module("src", run_name="__main__", alter_sys=True)
