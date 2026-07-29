#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Polyfills are tested independently: this executable links only the polyfills
# library and the test framework (no mrdocs-core, no LLVM).
file(GLOB_RECURSE POLYFILLS_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(mrdocs-polyfill-tests ${POLYFILLS_TEST_SOURCES})
target_link_libraries(mrdocs-polyfill-tests PRIVATE mrdocs-polyfill test_suite)
add_test(NAME mrdocs-polyfill-tests COMMAND mrdocs-polyfill-tests)
