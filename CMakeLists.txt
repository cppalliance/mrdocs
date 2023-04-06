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

# VFALCO this is to link optimized llvm for debug builds
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "")

project(
        MrDox
        VERSION 1.0.0
        DESCRIPTION "C++ Documentation Tool"
        HOMEPAGE_URL "https://github.com/cppalliance/mrdox"
        LANGUAGES CXX C
)
option(MRDOX_BUILD_TESTS "Build tests" ${BUILD_TESTING})

#-------------------------------------------------
#
# Dependencies
#
#-------------------------------------------------

function(get_llvm_dir_for_build_type build_type output_var)
  if(${build_type} STREQUAL "Debug")
    set(llvm_dir ${LLVM_DEBUG_PATH})
  elseif(${build_type} STREQUAL "DebugRel")
    set(llvm_dir ${LLVM_DEBUGREL_PATH})
  elseif(${build_type} STREQUAL "RelWithDebInfo")
    set(llvm_dir ${LLVM_RELWITHDEBINFO_PATH})
  else()
    message(FATAL_ERROR "Unsupported build type: ${build_type}")
  endif()

  set(${output_var} ${llvm_dir} PARENT_SCOPE)
endfunction()

get_llvm_dir_for_build_type(${CMAKE_BUILD_TYPE} LLVM_DIR)

set(CMAKE_FOLDER Dependencies)

find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_DIR})
find_package(Clang REQUIRED CONFIG PATHS ${LLVM_DIR})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
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
target_include_directories(mrdox_lib PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source/lib>")

if (TARGET LLVM)
    target_link_libraries(mrdox_lib PRIVATE LLVM)
else ()
    include(HandleLLVMOptions)
    llvm_map_components_to_libnames(llvm_libs)
    target_link_libraries(mrdox_lib PUBLIC ${llvm_libs})
endif ()
target_include_directories(mrdox_lib PUBLIC ${LLVM_INCLUDE_DIRS})
target_compile_definitions(mrdox_lib PUBLIC ${LLVM_DEFINITIONS})

if (${LLVM_VERSION} VERSION_GREATER_EQUAL "10.0.0")
    target_link_libraries(mrdox_lib PRIVATE clang-cpp)
else ()
    target_link_libraries(
            mrdox_lib
            PUBLIC
            clangAST
            clangBasic
            clangFrontend
            clangIndex
            clangTooling
            clangToolingCore
            clangToolingInclusions
    )
endif ()
target_include_directories(mrdox_lib PUBLIC ${CLANG_INCLUDE_DIRS})

if (WIN32)
    target_compile_definitions(
            mrdox_lib
            PUBLIC
            -D_WIN32_WINNT=0x0601
            -D_CRT_SECURE_NO_WARNINGS
            -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
    )
endif ()

if (MSVC)
    target_compile_options(
            mrdoc_lib
            PUBLIC
            /permissive-    # strict C++
            /W4             # enable all warnings
            /MP             # multi-processor compilation
            /EHsc           # C++ Exception handling
    )
    target_compile_definitions(
            mrdox_lib
            PUBLIC
            -D_ITERATOR_DEBUG_LEVEL=0
    )
endif ()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "" FILES ${LIB_INCLUDES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/lib PREFIX "source" FILES ${LIB_SOURCES})

#-------------------------------------------------
#
# Tool
#
#-------------------------------------------------

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS source/mrdox/ToolMain.cpp)

add_executable(mrdox ${TOOL_SOURCES})
target_link_libraries(mrdox PRIVATE mrdox_lib)
target_include_directories(mrdox PRIVATE ${PROJECT_SOURCE_DIR}/source/mrdox)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/mrdox PREFIX "" FILES ${TOOL_SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

if (MRDOX_BUILD_TESTS)
    enable_testing()
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS source/tests/*.cpp)
    add_executable(mrdox_tests ${TEST_SOURCES})
    target_link_libraries(mrdox_tests PRIVATE mrdox_lib)
    target_include_directories(mrdox_tests PRIVATE ${PROJECT_SOURCE_DIR}/source/tests)
    add_test(NAME mrdox_tests COMMAND mrdox_tests "${CMAKE_CURRENT_SOURCE_DIR}/testfiles")
    add_test(NAME mrdox_tests_bare COMMAND mrdox_tests "${CMAKE_CURRENT_SOURCE_DIR}/testfiles/bare")
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source/tests PREFIX "" FILES ${TEST_SOURCES})
endif ()

#-------------------------------------------------
