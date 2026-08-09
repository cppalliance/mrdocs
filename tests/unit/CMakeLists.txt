#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Unit tests: own executable, no golden harness, public API + test_suite only.
file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
add_executable(mrdocs-unit-tests ${UNIT_TEST_SOURCES})
target_include_directories(mrdocs-unit-tests
        PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        PRIVATE
        # Unit tests exercise private library internals. Expose the source
        # (and generated) tree so they include those as <mrdocs/...> rather
        # than reaching in with ../../src/mrdocs/... relative paths. The
        # public include/ above is searched first, so public headers win.
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}/src"
        )
target_link_libraries(mrdocs-unit-tests PUBLIC mrdocs-core test_suite)
# Unit tests exercise MrDocs's private headers, some of which include LLVM.
target_link_libraries(mrdocs-unit-tests PRIVATE mrdocs-llvm)
if (MRDOCS_CLANG)
    target_compile_options(mrdocs-unit-tests PRIVATE -Wno-covered-switch-default)
endif ()
target_compile_definitions(mrdocs-unit-tests PRIVATE -DMRDOCS_TEST_FILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME mrdocs-unit-tests COMMAND mrdocs-unit-tests)
