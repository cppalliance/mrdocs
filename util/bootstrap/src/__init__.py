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
MrDocs Bootstrap Tool.

A tool for setting up the MrDocs development environment, installing
dependencies, and configuring build presets.
"""

__version__ = "1.0.0"

from .installer import MrDocsInstaller
from .core import InstallOptions
