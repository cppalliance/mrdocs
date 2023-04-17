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

cmake_minimum_required(VERSION 3.13)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(
    MrDox
    VERSION 1.0.0
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdox"
    LANGUAGES CXX C
)

set(BUILD_SHARED_LIBS OFF CACHE STRING "")
set(CMAKE_CXX_EXTENSIONS OFF CACHE STRING "")
# VFALCO this is to link optimized llvm for debug builds
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON CACHE STRING "")
option(MRDOX_BUILD_TESTS "Build tests" ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(MRDOX_GCC ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
    set(MRDOX_CLANG ON)
endif()

if (WIN32)
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING)
    if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32") # 32-bit
        add_compile_options(
            /arch:SSE2
        )
    elseif("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64") # 64-bit
        add_compile_options(
            /arch:AMD64
        )
    endif()
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

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------

file(GLOB_RECURSE LIB_INCLUDES CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    source/lib/*.h
    source/lib/*.hpp
    source/lib/*.cpp
    source/lib/*.natvis)

add_library(mrdox_lib ${LIB_INCLUDES} ${LIB_SOURCES})
target_compile_features(mrdox_lib PUBLIC cxx_std_20)
target_include_directories(mrdox_lib
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/source/lib/"
)

# LLVM
target_link_libraries(mrdox_lib PUBLIC ${llvm_libs})
target_include_directories(mrdox_lib SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})

# Clang
if (CLANG_SIMPLE_LIBS)
    target_link_libraries(mrdox_lib PUBLIC LLVM clang clang-cpp)
else()
    target_link_libraries(mrdox_lib
        PUBLIC
        clangAST
        clangBasic
        clangFrontend
        clangIndex
        clangTooling
        clangToolingCore
        clangToolingInclusions)
endif()
target_include_directories(mrdox_lib SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})

# Windows, Win64
if (WIN32)
    target_compile_definitions(
        mrdox_lib
        PUBLIC
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
    )
    get_target_property(LLVM_CONFIGURATION_TYPE LLVMCore IMPORTED_CONFIGURATIONS)
    if (LLVM_CONFIGURATION_TYPE STREQUAL RELWITHDEBINFO)
        target_compile_definitions(mrdox_lib PUBLIC -D_ITERATOR_DEBUG_LEVEL=0)
        target_compile_options(mrdox_lib PUBLIC /MD)
    endif()
    if(MSVC)
        target_compile_options(
            mrdox_lib
            PUBLIC
            /permissive-    # strict C++
            /W4             # enable all warnings
            /MP             # multi-processor compilation
            /EHsc           # C++ Exception handling
        )
    endif()
endif ()

if (MRDOX_CLANG)
    target_compile_options(
        mrdox_lib
        PUBLIC
        -Wno-unused-private-field
        -Wno-unused-value
        PRIVATE
        -Wno-covered-switch-default
    )
endif ()

source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${PROJECT_SOURCE_DIR}/include/mrdox PREFIX "include" FILES ${LIB_INCLUDES})
source_group(TREE ${PROJECT_SOURCE_DIR}/source/lib PREFIX "source" FILES ${LIB_SOURCES})

#-------------------------------------------------
#
# Tool
#
#-------------------------------------------------

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS source/tool/*.cpp source/tool/*.hpp)
add_executable(mrdox ${TOOL_SOURCES})
target_link_libraries(mrdox PRIVATE mrdox_lib)
target_include_directories(mrdox PRIVATE source/tool)

source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${PROJECT_SOURCE_DIR}/source/tool PREFIX "source" FILES ${TOOL_SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

if (MRDOX_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS source/tests/*.cpp source/tests/*.hpp)
    enable_testing()
    add_executable(mrdox_tests ${TEST_SOURCES})
    target_link_libraries(mrdox_tests PRIVATE mrdox_lib ${llvm_libs})
    target_include_directories(mrdox_tests PRIVATE ${PROJECT_SOURCE_DIR}/source/tests)
    add_test(NAME mrdox_tests COMMAND mrdox_tests
        "${PROJECT_SOURCE_DIR}/tests/decls"
        "${PROJECT_SOURCE_DIR}/tests/javadoc"
    )
    source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
    source_group(TREE ${PROJECT_SOURCE_DIR}/source/tests PREFIX "source" FILES ${TEST_SOURCES})
endif()

#-------------------------------------------------
