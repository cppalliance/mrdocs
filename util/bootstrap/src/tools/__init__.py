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
Tool detection and installation for the bootstrap process.

This package provides functions to detect, validate, and install
various build tools required by MrDocs.
"""

from .detection import *
from .compilers import *
from .ninja import *
from .visual_studio import *
from .java import *
