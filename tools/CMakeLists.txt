#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The tools link mrdocs-core, which the header-only "MrDocs build" does not
# build, so skip tools/ in that mode.
if (MRDOCS_BUILD_HEADERS_ONLY)
    return()
endif()

# Command-line tools. Each tool lives in its own subdirectory with its own
# CMakeLists; the mrdocs CLI is the only one so far.
add_subdirectory(mrdocs)
