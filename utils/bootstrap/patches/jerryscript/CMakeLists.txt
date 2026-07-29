#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Minimal CMake packaging wrapper for JerryScript
# Builds a static jerry-core with ES.next profile, no tools/debugger/snapshots.

cmake_minimum_required(VERSION 3.16)
project(jerryscript VERSION 3.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(JERRY_PROFILE "es.next" CACHE STRING "JerryScript profile (es5.1 | es.next)")
option(JERRY_DEBUGGER "Build JerryScript debugger" OFF)
option(JERRY_SNAPSHOT_SAVE "Enable snapshot saving" OFF)
option(JERRY_SNAPSHOT_EXEC "Enable snapshot execution" OFF)
option(JERRY_CMDLINE "Build command-line shell" OFF)
option(JERRY_TESTS "Build tests" OFF)
option(JERRY_MEM_STATS "Enable memory statistics" OFF)
option(JERRY_PARSER_STATS "Enable parser statistics" OFF)
option(JERRY_LINE_INFO "Enable line info" OFF)
option(JERRY_LTO "Enable LTO" OFF)
option(JERRY_LIBC "Use bundled libc" OFF)
option(JERRY_PORT "Build default port implementation" ON)

# Minimal platform/compiler detection to satisfy jerry-port CMake expectations.
set(PLATFORM "${CMAKE_SYSTEM_NAME}")
string(TOUPPER "${PLATFORM}" PLATFORM)

if(MSVC)
    set(USING_MSVC 1)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(USING_GCC 1)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(USING_CLANG 1)
endif()

if (MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
endif()

# Upstream expects an amalgam target; stub it.
add_custom_target(amalgam)

# Build jerry-core using upstream CMakeLists
add_subdirectory(jerry-core)

# Build jerry-port ourselves to exclude context functions when JERRY_EXTERNAL_CONTEXT=ON.
# When external context is enabled, the host application must provide its own
# implementations of jerry_port_context_alloc, jerry_port_context_free, and
# jerry_port_context_get. By excluding jerry-port-context.c from the library,
# we avoid ODR violations and make the design explicit.
if (JERRY_PORT)
    set(JERRY_PORT_SOURCES
        jerry-port/common/jerry-port-fs.c
        jerry-port/common/jerry-port-io.c
        jerry-port/common/jerry-port-process.c
        jerry-port/unix/jerry-port-unix-date.c
        jerry-port/unix/jerry-port-unix-fs.c
        jerry-port/unix/jerry-port-unix-process.c
        jerry-port/win/jerry-port-win-date.c
        jerry-port/win/jerry-port-win-fs.c
        jerry-port/win/jerry-port-win-process.c
    )

    # Include context functions only when NOT using external context
    if (NOT JERRY_EXTERNAL_CONTEXT)
        list(APPEND JERRY_PORT_SOURCES jerry-port/common/jerry-port-context.c)
    endif()

    add_library(jerry-port ${JERRY_PORT_SOURCES})
    set_target_properties(jerry-port PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(jerry-port PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/jerry-core/include
    )
    target_compile_definitions(jerry-port PRIVATE
        _BSD_SOURCE
        _DEFAULT_SOURCE
        JERRY_GLOBAL_HEAP_SIZE=${JERRY_GLOBAL_HEAP_SIZE}
    )
endif()

set_target_properties(jerry-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(jerry-core PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/jerry-core/include>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

add_library(jerryscript ALIAS jerry-core)

set(_install_targets jerry-core)
if (TARGET jerry-port)
    list(APPEND _install_targets jerry-port)
endif()

install(TARGETS ${_install_targets}
    EXPORT jerryscriptTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/jerry-core/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/jerryscriptConfig.cmake.in
    ${PROJECT_BINARY_DIR}/jerryscriptConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jerryscript
)

install(EXPORT jerryscriptTargets
    NAMESPACE jerryscript::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jerryscript)

install(FILES ${PROJECT_BINARY_DIR}/jerryscriptConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jerryscript)
