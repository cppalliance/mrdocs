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
IDE configuration generation for the bootstrap process.

This package provides functionality to generate run configurations
for various IDEs (CLion, VSCode, Visual Studio) and debugger
pretty-printer configurations.
"""

from .run_configs import *
from .clion import *
from .vscode import *
from .visual_studio import *
from .pretty_printers import *
