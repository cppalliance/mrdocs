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
Core infrastructure shared across all bootstrap modules.

This package contains UI utilities, configuration options, platform
detection, and common filesystem operations.
"""

from .ui import *
from .platform import *
from .options import *
from .filesystem import *
from .process import *
from .prompts import *
from .git import *
