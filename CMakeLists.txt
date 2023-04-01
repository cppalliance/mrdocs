#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdox
#

cmake_minimum_required(VERSION 3.25.2)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreadedDLL" CACHE STRING "")

#-------------------------------------------------

project(
    MrDox
    VERSION 1.0.0
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdox"
    LANGUAGES CXX
)

#-------------------------------------------------
#
# Toolchain settings
#
#-------------------------------------------------

set(BUILD_SHARED_LIBS OFF CACHE STRING "")
set(CMAKE_CXX_EXTENSIONS OFF CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON CACHE STRING "")

# Windows, Win64
if(WIN32)
    add_definitions(
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
    )
endif()

# cl.exe
if(MSVC)
    add_compile_options(
        /permissive-    # strict C++
        /W4             # enable all warnings
        /MP             # multi-processor compilation
        /EHsc           # C++ Exception handling
    )
    add_definitions(
        -D_ITERATOR_DEBUG_LEVEL=0
    )
endif()

#-------------------------------------------------

file(GLOB_RECURSE LIB_INCLUDES CONFIGURE_DEPENDS
    include/*.hpp
)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    source/lib/*.h
    source/lib/*.hpp
    source/lib/*.cpp
    source/lib/*.natvis
)

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS
    source/mrdox/*.h
    source/mrdox/*.hpp
    source/mrdox/*.cpp
)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    source/tests/*.cpp
)

#-------------------------------------------------

set(CMAKE_FOLDER Dependencies)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)

add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs)

unset(CMAKE_FOLDER)

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------

add_library(
    mrdox_lib
    ${LIB_INCLUDES}
    ${LIB_SOURCES}
)

target_compile_features(
    mrdox_lib
    PUBLIC
    cxx_std_20)

target_include_directories(
    mrdox_lib
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source/lib>" # VFALCO THIS IS HACK, FIX ASAP
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/source/lib
)

target_link_libraries(
    mrdox_lib
    PUBLIC
    ${llvm_libs}
    clangAST
    clangBasic
    clangFrontend
    clangIndex
    clangTooling
    clangToolingCore
    clangToolingInclusions
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "" FILES ${LIB_INCLUDES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/lib PREFIX "source" FILES ${LIB_SOURCES})

#-------------------------------------------------
#
# Tool
#
#-------------------------------------------------

add_executable(
    mrdox
    ${TOOL_SOURCES})

target_link_libraries(
    mrdox
    PRIVATE
    mrdox_lib)

target_include_directories(
    mrdox
    PRIVATE
    ${PROJECT_SOURCE_DIR}/source/mrdox
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/mrdox PREFIX "" FILES ${TOOL_SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

add_executable(
    mrdox_tests
    ${TEST_SOURCES}
)

target_link_libraries(
    mrdox_tests
    PRIVATE
    mrdox_lib)

target_include_directories(
    mrdox_tests
    PRIVATE
    ${PROJECT_SOURCE_DIR}/source/tests
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/tests PREFIX "" FILES ${TEST_SOURCES})

#-------------------------------------------------
