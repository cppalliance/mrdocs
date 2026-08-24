#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

#-------------------------------------------------
#
# Project data
#
#-------------------------------------------------
cmake_minimum_required(VERSION 3.13)
project(
    MrDocs
    VERSION 0.8.0
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdocs"
    LANGUAGES CXX C
)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
include(utils/cmake/helpers.cmake)

#-------------------------------------------------
#
# Project options
#
#-------------------------------------------------
# What to build
option(MRDOCS_MRDOCS_BUILD "Build only public-headers for documentation self-reference" ${MRDOCS_BUILD})
if (MRDOCS_MRDOCS_BUILD)
    include(docs/mrdocs-build.cmake)
    return()
endif()
option(MRDOCS_BUILD_TESTS "Build tests" ${BUILD_TESTING})
option(MRDOCS_BUILD_STRICT_TESTS "Enable costly strict tests" ON)
option(MRDOCS_BUILD_EXAMPLES "Build the examples" ${MRDOCS_BUILD_TESTS})
option(MRDOCS_BUILD_DOCS "Build documentation" OFF)
option(MRDOCS_GENERATE_REFERENCE "Generate MrDocs reference" ${MRDOCS_BUILD_DOCS})
option(MRDOCS_INSTALL "Configure install target" ON)
option(MRDOCS_PACKAGE "Build install package" ON)
if (MRDOCS_BUILD_TESTS OR MRDOCS_BUILD_EXAMPLES)
    enable_testing()
    include(CTest)
endif()

# How to build
option(MRDOCS_REQUIRE_GIT "Git is required: not being able to extract version build is an error" ON)
option(MRDOCS_BUNDLE_DEPENDENCIES "Bundle the private dependencies into the library" ON)
option(MRDOCS_BUILD_SHARED "Link shared" ${BUILD_SHARED_LIBS})
option(MRDOCS_GENERATE_ANTORA_REFERENCE "Generate MrDocs reference in Antora module pages" OFF)

# Helper variables based on options
set_ternary(MRDOCS_LINK_MODE MRDOCS_BUILD_SHARED SHARED "")
set_ternary(MRDOCS_LINK_MODE_DEFINITION MRDOCS_BUILD_SHARED MRDOCS_SHARED_LINK MRDOCS_STATIC_LINK)
set_ternary(MRDOCS_DO_BUNDLE_DEPENDENCIES "MRDOCS_BUNDLE_DEPENDENCIES AND NOT MRDOCS_BUILD_SHARED" ON OFF)
set_ternary(MRDOCS_DO_REEXPORT_DEPENDENCIES "NOT MRDOCS_DO_BUNDLE_DEPENDENCIES AND NOT MRDOCS_BUILD_SHARED" ON OFF)
set_ternary(MRDOCS_GCC   "CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\"" ON OFF)
set_ternary(MRDOCS_CLANG "CMAKE_CXX_COMPILER_ID MATCHES \"Clang$\"" ON OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# MSVC flags shared by every mrdocs target below (not installed consumers):
#  - /EHs enables C++ exception handling. The mrdocs tool, the unit tests, and the
#    examples are separate targets whose TUs include exception-using STL headers, so
#    without it MSVC raises C4530 and /WX makes it fatal; the EH model must also be
#    uniform across all TUs, so it belongs here rather than on mrdocs-core alone.
#  - /wd4100 accepts unused parameters, an accepted style across mrdocs (visitor /
#    tag_invoke / finalizer signatures) that the clang build already allows via
#    -Wno-unused-parameter; mirror it so /W4 /WX does not fail on C4100.
if (MSVC)
    add_compile_options(/EHs /wd4100)
endif()

#-------------------------------------------------
#
# Project Components
#
#-------------------------------------------------
add_subdirectory(utils)
add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(tools)
if (MRDOCS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if (MRDOCS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if (MRDOCS_BUILD_DOCS)
    add_subdirectory(docs)
endif()
if (MRDOCS_INSTALL)
    add_subdirectory(data)
    mrdocs_install()
endif()
