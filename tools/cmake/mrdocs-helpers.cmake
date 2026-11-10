#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Helpers for the projects that link MrDocs. This file is installed next to
# mrdocs-config.cmake, which includes it, and the internal MrDocs build
# includes it too, so the same call works in both. Each helper is in a file of
# its own, named after it.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/mrdocs_ensure_runtime_dlls.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/mrdocs_add_plugin.cmake")
