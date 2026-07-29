#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

if (MRDOCS_BUILD_HEADERS_ONLY OR NOT MRDOCS_BUILD_EXAMPLES)
    return()
endif()

# Each example subdirectory owns the tests that exercise it.
add_subdirectory(generators)      # data-driven + script-driven generator examples
add_subdirectory(library)         # standalone programs that link mrdocs-core
add_subdirectory(configuration)   # per-option configuration examples from the docs
add_subdirectory(getting-started) # starter projects invoked as the docs instruct
add_subdirectory(dependencies)    # dependency-handling fixtures
