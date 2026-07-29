#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Handlebars behavior tests: link only the handlebars library and the test
# framework. No fixtures, no LLVM.
file(GLOB_RECURSE HANDLEBARS_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(mrdocs-handlebars-unit-tests ${HANDLEBARS_UNIT_TEST_SOURCES})
target_link_libraries(mrdocs-handlebars-unit-tests
        PRIVATE mrdocs::handlebars test_suite)
add_test(NAME mrdocs-handlebars-unit-tests COMMAND mrdocs-handlebars-unit-tests)
