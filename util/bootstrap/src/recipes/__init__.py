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
Recipe-driven dependency management for the bootstrap process.

This package provides functionality to load, fetch, and build
third-party dependencies defined in recipe JSON files.
"""

from .schema import *
from .loader import *
from .fetcher import *
from .builder import *
from .archive import *
