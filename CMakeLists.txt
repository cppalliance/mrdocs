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
option(MRDOCS_INSTALL "Configure install target" ON)
option(MRDOCS_PACKAGE "Build install package" ON)
option(MRDOCS_BUILD_SHARED "Link shared" ${BUILD_SHARED_LIBS})
option(MRDOCS_BUILD_TESTS "Build tests" ${BUILD_TESTING})
option(MRDOCS_BUILD_EXAMPLES "Build the examples" ${MRDOCS_BUILD_TESTS})
if (MRDOCS_BUILD_TESTS OR MRDOCS_BUILD_EXAMPLES)
    enable_testing()
    include(CTest)
endif()
option(MRDOCS_BUILD_STRICT_TESTS "Enable strict tests" ON)
option(MRDOCS_REQUIRE_GIT "Git is required: not being able to extract version build is an error" ON)
option(MRDOCS_BUILD_DOCS "Build documentation" OFF)
option(MRDOCS_BUILD_HEADERS_ONLY "Build only public-headers for self-reference" OFF)
option(MRDOCS_GENERATE_REFERENCE "Generate MrDocs reference" ${MRDOCS_BUILD_DOCS})
option(MRDOCS_GENERATE_ANTORA_REFERENCE "Generate MrDocs reference in Antora module pages" OFF)

set_ternary(MRDOCS_LINK_MODE MRDOCS_BUILD_SHARED SHARED "")
set_ternary(MRDOCS_LINK_MODE_DEFINITION MRDOCS_BUILD_SHARED MRDOCS_SHARED_LINK MRDOCS_STATIC_LINK)
set_ternary(MRDOCS_GCC   "CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\"" ON OFF)
set_ternary(MRDOCS_CLANG "CMAKE_CXX_COMPILER_ID MATCHES \"Clang$\"" ON OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


if (MSVC)
    # Suppressions we use to get from LLVM's HandleLLVMOptions.cmake
    add_compile_options(/wd4100 /wd4127 /wd4141 /wd4146 /wd4204 /wd4244 /wd4245 /wd4267 /wd4291 /wd4310 /wd4319 /wd4324 /wd4351 /wd4389 /wd4456 /wd4457 /wd4458 /wd4459 /wd4503 /wd4505 /wd4510 /wd4512 /wd4577 /wd4592 /wd4610 /wd4611 /wd4624 /wd4701 /wd4702 /wd4703 /wd4706 /wd4722 /wd4805 /wd5105)
    # Conformance flags we used to get from LLVM's HandleLLVMOptions.cmake
    add_compile_options(/Zc:__cplusplus /Zc:preprocessor)
endif()

#-------------------------------------------------
#
# Project Components
#
#-------------------------------------------------
add_subdirectory(utils)
add_subdirectory(libs)
add_subdirectory(src)
if (MRDOCS_BUILD_HEADERS_ONLY)
    return()
else()
add_subdirectory(tools)
add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(docs)
add_subdirectory(data)
include(utils/cmake/install.cmake)
mrdocs_install()
endif()
