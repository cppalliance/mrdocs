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
# 3.18 is the lowest that has `cmake_language(EVAL)`, which `set_ternary` in
# utils/cmake/helpers/set_ternary.cmake uses. It also turns on policy CMP0099, which
# src/CMakeLists.txt needs for `mrdocs-hidden-archives`.
cmake_minimum_required(VERSION 3.18)
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
include(tools/cmake/mrdocs-helpers.cmake)

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
option(MRDOCS_REQUIRE_GIT "Git is required to configure unless MRDOCS_BUILD_ID is set or the source is a git archive with git-archival.txt expanded" ON)
option(MRDOCS_BUNDLE_DEPENDENCIES "Bundle the private dependencies into the library" ON)
option(MRDOCS_BUILD_SHARED "Link shared" ${BUILD_SHARED_LIBS})
option(MRDOCS_GENERATE_ANTORA_REFERENCE "Generate MrDocs reference in Antora module pages" OFF)

# Helper variables based on options
set_ternary(MRDOCS_DO_BUNDLE_DEPENDENCIES "MRDOCS_BUNDLE_DEPENDENCIES AND NOT MRDOCS_BUILD_SHARED" ON OFF)
set_ternary(MRDOCS_DO_REEXPORT_DEPENDENCIES "NOT MRDOCS_DO_BUNDLE_DEPENDENCIES AND NOT MRDOCS_BUILD_SHARED" ON OFF)
set_ternary(MRDOCS_GCC   "CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\"" ON OFF)
set_ternary(MRDOCS_CLANG "CMAKE_CXX_COMPILER_ID MATCHES \"Clang$\"" ON OFF)

# Bundles the C++ runtime for the Linux release package.
option(MRDOCS_BUNDLE_CXX_RUNTIME "Bundle the C++ runtime for the Linux release package (libstdc++ and libgcc_s in lib/, RUNPATH $ORIGIN/../lib, no -static); a local build leaves it off" OFF)
if (MRDOCS_BUNDLE_CXX_RUNTIME)
    # The file holds the settings and what each is for.
    include(utils/cmake/release-linux.cmake)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# The standard library tag of this compiler. A plugin is given it to check its
# own settings at its first compile.
mrdocs_detect_stdlib_tag(MRDOCS_STDLIB_TAG_DEFINITIONS MRDOCS_STDLIB_TAG)

if (MSVC)
    # Exception handling, uniform across every target, as the STL headers need it.
    add_compile_options(/EHs)
    # Accepts unused parameters, as the clang build does.
    add_compile_options(/wd4100)
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
