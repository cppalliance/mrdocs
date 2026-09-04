#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# If not inside the Mr.Docs source tree.
if (NOT TARGET mrdocs::mrdocs-core)
    cmake_minimum_required(VERSION 3.20)
    project(mrdocs-breaking-changes-example LANGUAGES CXX)
    # tag::package[]
    find_package(mrdocs REQUIRED CONFIG)
    # end::package[]
    include(CTest)
endif ()

# tag::target[]
add_executable(mrdocs-breaking-changes-example
    src/main.cpp
    src/Corpus.cpp
    src/Diff.cpp
    src/BreakingChangesGenerator.cpp
)
target_link_libraries(mrdocs-breaking-changes-example PRIVATE mrdocs::mrdocs-core)
# end::target[]
if (MRDOCS_CLANG)
    target_compile_options(mrdocs-breaking-changes-example PRIVATE -Wno-covered-switch-default)
endif()

if (BUILD_TESTING)
    add_test(
        NAME mrdocs-library-example-breaking-changes
        COMMAND ${CMAKE_COMMAND}
                "-DTOOL=$<TARGET_FILE:mrdocs-breaking-changes-example>"
                "-DV1_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/test/v1/mrdocs.yml"
                "-DV2_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/test/v2/mrdocs.yml"
                "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/test/expected.txt"
                "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/actual.txt"
                "-DBUILTIN_ARGS=${MRDOCS_BUILTIN_DIR_ARGS}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/test/run.cmake"
    )
endif()
