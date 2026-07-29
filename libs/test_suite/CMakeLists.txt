#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The unit-test framework: a standalone, self-contained library (no mrdocs/LLVM
# deps) with its own recursive include/ + src/ layout. The test suites under
# tests/ link it and consume it as <test_suite/...> via its public include dir.
file(GLOB_RECURSE TEST_SUITE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
add_library(test_suite STATIC ${TEST_SUITE_SOURCES})
add_library(mrdocs::test_suite ALIAS test_suite)
target_include_directories(test_suite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(test_suite PUBLIC cxx_std_23)
