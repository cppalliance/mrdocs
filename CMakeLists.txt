#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdox
#

#-------------------------------------------------
#
# Project settings
#
#-------------------------------------------------

cmake_minimum_required(VERSION 3.20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# VFALCO this is to link optimized llvm for debug builds
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "")

project(
    MrDox
    VERSION 1.0.0
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdox"
    LANGUAGES CXX C
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
#
# Dependencies
#
#-------------------------------------------------

set(CMAKE_FOLDER Dependencies)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support core irreader)
unset(CMAKE_FOLDER)

if (LLVM_FOUND)
    message(STATUS "LLVM found in ${LLVM_INCLUDE_DIRS}")
endif ()
if (Clang_FOUND)
    message(STATUS "Clang found in ${CLANG_INCLUDE_DIRS}")
endif ()

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------

file(GLOB_RECURSE LIB_INCLUDES CONFIGURE_DEPENDS
    include/*.hpp)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    source/lib/*.h
    source/lib/*.hpp
    source/lib/*.cpp
    source/lib/*.natvis)

if (CLANG_SIMPLE_LIBS)
    set(clang_libs clang clang-cpp)
else()
    set(clang_libs
        clangAST
        clangBasic
        clangFrontend
        clangIndex
        clangTooling
        clangToolingCore
        clangToolingInclusions)
endif()

add_library(mrdox_lib ${LIB_INCLUDES} ${LIB_SOURCES})
target_compile_features(mrdox_lib PUBLIC cxx_std_20)
target_include_directories(mrdox_lib PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    # VFALCO source/lib shouldn't be here it is a hack
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source/lib>"
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    PRIVATE
    "${PROJECT_SOURCE_DIR}/source/lib")

target_link_libraries(
    mrdox_lib
    PUBLIC
    LLVM
    ${llvm_libs}
    ${clang_libs})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "" FILES ${LIB_INCLUDES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/lib PREFIX "source" FILES ${LIB_SOURCES})

#-------------------------------------------------
#
# Tool
#
#-------------------------------------------------

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS
    source/tool/*.cpp
    source/tool/*.hpp)

add_executable(mrdox ${TOOL_SOURCES})
target_link_libraries(mrdox PRIVATE mrdox_lib)

target_include_directories(
    mrdox
    PRIVATE
    ${PROJECT_SOURCE_DIR}/source/tool)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/tool PREFIX "" FILES ${TOOL_SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    source/tests/*.cpp
    source/tests/*.hpp)
enable_testing()
add_executable(mrdox_tests ${TEST_SOURCES})
target_link_libraries(mrdox_tests PRIVATE mrdox_lib ${llvm_libs})
target_include_directories(mrdox_tests PRIVATE ${PROJECT_SOURCE_DIR}/source/tests)
add_test(NAME mrdox_tests COMMAND mrdox_tests "${CMAKE_CURRENT_SOURCE_DIR}/testfiles")
add_test(NAME mrdox_tests_bare COMMAND mrdox_tests "${CMAKE_CURRENT_SOURCE_DIR}/testfiles/bare")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/tests PREFIX "" FILES ${TEST_SOURCES})

#-------------------------------------------------
