#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# dom is tested independently: this executable links only the dom library, the
# polyfills it depends on, and the test framework (no mrdocs-core, no LLVM).
file(GLOB_RECURSE DOM_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(mrdocs-dom-tests ${DOM_TEST_SOURCES})
target_link_libraries(mrdocs-dom-tests PRIVATE mrdocs::dom test_suite)
add_test(NAME mrdocs-dom-tests COMMAND mrdocs-dom-tests)
