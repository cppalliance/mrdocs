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
        DESCRIPTION "C++ Javadoc Extraction Tool"
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

# x86
#if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32") # 32-bit
#    add_compile_options(
#        /arch:SSE2
#    )
#endif()

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

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    src/*.h
    src/*.hpp
    src/*.ipp
    src/*.natvis
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_executable(
    mrdox
    ${HEADERS}
    ${SOURCES})

target_compile_definitions(
    mrdox
    PUBLIC
    )

#-------------------------------------------------

set(CMAKE_FOLDER Dependencies)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs
    )

target_include_directories(
    mrdox
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/>"
    PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_link_libraries(mrdox
    PRIVATE
    ${llvm_libs}
    clangAST
    clangBasic
    clangFrontend
    clangIndex
    clangTooling
    clangToolingCore
    clangToolingInclusions
)

unset(CMAKE_FOLDER)

#-------------------------------------------------

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "" FILES ${HEADERS} ${SOURCES})

set_property(TARGET mrdox PROPERTY OUTPUT_NAME mrdox)

target_compile_features(mrdox
    PRIVATE
    cxx_std_20)
