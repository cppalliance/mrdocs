#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#
#

#
# This file is derived from the CMakeLists.txt file in the Microsoft vcpkg repository:
# https://github.com/microsoft/vcpkg/blob/master/ports/duktape/CMakeLists.txt
#

cmake_minimum_required(VERSION 3.13)

set(duktape_MAJOR_VERSION 2)
set(duktape_MINOR_VERSION 7)
set(duktape_PATCH_VERSION 0)
set(duktape_VERSION ${duktape_MAJOR_VERSION}.${duktape_MINOR_VERSION}.${duktape_PATCH_VERSION})

option(CMAKE_VERBOSE_MAKEFILE "Create verbose makefile" OFF)
option(BUILD_SHARED_LIBS "Create duktape as a shared library" OFF)

project(duktape VERSION ${duktape_VERSION})

file(GLOB_RECURSE DUKTAPE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
file(GLOB_RECURSE DUKTAPE_HEADERS "${CMAKE_CURRENT_LIST_DIR}/src/*.h")

add_library(duktape ${DUKTAPE_SOURCES} ${DUKTAPE_HEADERS})
target_include_directories(duktape PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
set_target_properties(duktape PROPERTIES PUBLIC_HEADER "${DUKTAPE_HEADERS}")
set_target_properties(duktape PROPERTIES VERSION ${duktape_VERSION})
set_target_properties(duktape PROPERTIES SOVERSION ${duktape_MAJOR_VERSION})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(duktape PRIVATE DUK_F_DLL_BUILD)
endif ()

install(TARGETS duktape
        EXPORT duktapeTargets
        ARCHIVE DESTINATION "lib"
        LIBRARY DESTINATION "lib"
        RUNTIME DESTINATION "bin"
        PUBLIC_HEADER DESTINATION "include"
        COMPONENT dev
)

install(EXPORT duktapeTargets
        FILE duktapeTargets.cmake
        NAMESPACE duktape::
        DESTINATION "share/duktape"
)

export(PACKAGE duktape)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/duktapeConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
)

configure_file(duktapeConfig.cmake.in "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/duktapeConfig.cmake" @ONLY)

install(FILES
        "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/duktapeConfig.cmake"
        "${PROJECT_BINARY_DIR}/duktapeConfigVersion.cmake"
        DESTINATION "share/duktape"
)