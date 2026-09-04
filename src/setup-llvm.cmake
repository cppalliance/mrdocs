#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

#
# Find LLVM + Clang and create a wrapped mrdocs-llvm interface target.
#
include_guard(GLOBAL)

#-------------------------------------------------
# Ensure directories
#-------------------------------------------------
# Ensure <PackageName>_ROOT variables and give us a strong fatal error
# with a better explanation if they're missing. MrDocs requires an
# explicit path with our specific build. We never use an LLVM installation
# from the system.
if (NOT LLVM_ROOT AND DEFINED ENV{LLVM_ROOT})
    set(LLVM_ROOT "$ENV{LLVM_ROOT}")
endif()
if (LLVM_ROOT)
    get_filename_component(LLVM_ROOT "${LLVM_ROOT}" ABSOLUTE)
    set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "Root of LLVM install." FORCE)
    if (NOT EXISTS "${LLVM_ROOT}")
        message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) provided does not exist.")
    endif()
    if (NOT EXISTS "${LLVM_ROOT}/lib/cmake/llvm")
        message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) is invalid: no <LLVM_ROOT>/lib/cmake/llvm.")
    endif()
    message(STATUS "LLVM_ROOT: ${LLVM_ROOT}")
endif()
if (Clang_ROOT)
    get_filename_component(Clang_ROOT "${Clang_ROOT}" ABSOLUTE)
    set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "Root of Clang install." FORCE)
elseif (LLVM_ROOT)
    set(Clang_ROOT "${LLVM_ROOT}" CACHE PATH "Root of Clang install." FORCE)
endif()

#-------------------------------------------------
# Find packages
#-------------------------------------------------
# LLVM publishes headers and definitions as variables.
# There is no single interface target for them.
find_package(LLVM REQUIRED CONFIG)
# Clang gives per-component targets (clangTooling, clangAST, ...) that
# link LLVM libraries transitively but not the headers or definitions.
find_package(Clang REQUIRED CONFIG)
# Find libc++ headers, which are not part of the LLVM/Clang CMake packages. The
# include dir is always <LLVM_ROOT>/include/c++/v1.
set(LIBCXX_DIR "${LLVM_INCLUDE_DIR}/c++/v1" CACHE PATH "Path to libc++ include directory")
message(STATUS "LIBCXX_DIR: ${LIBCXX_DIR}")
if (NOT EXISTS "${LIBCXX_DIR}")
    message(FATAL_ERROR "LIBCXX_DIR (${LIBCXX_DIR}) does not exist. Provide an LLVM with libc++ enabled.")
endif()
# Clang's resource directory: builtin headers (stddef.h, stdarg.h, ...) LLVM ships
# at <LLVM_BINARY_DIR>/lib/clang/<major>.
set(CLANG_RESOURCE_DIR "${LLVM_BINARY_DIR}/lib/clang/${Clang_VERSION_MAJOR}"
        CACHE PATH "Path to clang's resource directory (builtin headers under include/)")
message(STATUS "CLANG_RESOURCE_DIR: ${CLANG_RESOURCE_DIR}")
if (NOT EXISTS "${CLANG_RESOURCE_DIR}/include")
    message(FATAL_ERROR "CLANG_RESOURCE_DIR (${CLANG_RESOURCE_DIR}) has no include/ subdirectory. Provide an LLVM install that contains the clang resource directory.")
endif()

#-------------------------------------------------
# Replay flags
#-------------------------------------------------
# Replay the flags LLVM was built with
# LLVM doesn't expose modular targets
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
# HandleLLVMOptions forces a /W level and a set of -wd suppressions into the global
# flags. Strip both: mrdocs-core sets its own /W4, and LLVM's header warnings are
# scoped to LLVM headers via /external:W0 on mrdocs-llvm below -- NOT suppressed
# project-wide, so real warnings in mrdocs's own code are never hidden.
if (MSVC)
    string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE " [-/]wd[0-9]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE " [-/]wd[0-9]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

#-------------------------------------------------
# Create interface target
#-------------------------------------------------
# Wrap LLVM/Clang's usage requirements into a modular interface target.
add_library(mrdocs-llvm INTERFACE)
target_include_directories(mrdocs-llvm SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${LLVM_INCLUDE_DIRS}>"
        "$<BUILD_INTERFACE:${CLANG_INCLUDE_DIRS}>")
target_compile_definitions(mrdocs-llvm INTERFACE ${LLVM_DEFINITIONS})
target_link_libraries(mrdocs-llvm INTERFACE clangAST clangBasic clangFrontend clangIndex clangTooling clangToolingCore clangToolingInclusions)
if (MSVC)
    # Mark LLVM warnings as external so they don't leak.
    # A few of the warnings couldn't be scoped, so they need an explicit /wd.
    #   - 4701/4702/4703 are codegen warnings
    #   - 4244/4245/4267 are narrowing warnings from STL templates that end up in <utility>
    target_compile_options(mrdocs-llvm INTERFACE /external:W0 /wd4701 /wd4702 /wd4703 /wd4244 /wd4245 /wd4267)
endif()
