#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#
#

cmake_minimum_required(VERSION 3.13)

set(lua_MAJOR_VERSION 5)
set(lua_MINOR_VERSION 4)
set(lua_PATCH_VERSION 8)
set(lua_VERSION ${lua_MAJOR_VERSION}.${lua_MINOR_VERSION}.${lua_PATCH_VERSION})

option(CMAKE_VERBOSE_MAKEFILE "Create verbose makefile" OFF)
option(BUILD_SHARED_LIBS "Create lua as a shared library" OFF)

project(lua VERSION ${lua_VERSION} LANGUAGES C)

# Files we always exclude from the core+stdlib library build:
#   - lua.c     standalone interpreter (defines its own `main`)
#   - luac.c    standalone bytecode compiler (defines its own `main`)
#   - onelua.c  unity-build alternative that #includes the others; its
#               default `MAKE_LUA` mode pulls in lua.c and would re-define
#               `main`, conflicting with any application that links lua.lib.
#   - ltests.c  Lua-internal test harness, only valid when LUA_USER_H
#               enables it.
set(LUA_EXCLUDE_NAMES lua.c luac.c onelua.c ltests.c)

# Check if ./src exists
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/src")
    set(LUA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
else()
    # Lua sources live under ./ (github release)
    set(LUA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

file(GLOB LUA_SOURCES "${LUA_SOURCE_DIR}/*.c")
foreach(_excl IN LISTS LUA_EXCLUDE_NAMES)
    list(REMOVE_ITEM LUA_SOURCES "${LUA_SOURCE_DIR}/${_excl}")
endforeach()
file(GLOB LUA_HEADERS "${LUA_SOURCE_DIR}/*.h")

add_library(lua ${LUA_SOURCES} ${LUA_HEADERS})
target_include_directories(lua
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(lua PROPERTIES
        PUBLIC_HEADER "${LUA_HEADERS}"
        VERSION ${lua_VERSION}
        SOVERSION ${lua_MAJOR_VERSION}
)

# Optional compatibility defines (uncomment if you need them)
# target_compile_definitions(lua PRIVATE LUA_COMPAT_5_3)

install(TARGETS lua
        EXPORT LuaTargets
        ARCHIVE DESTINATION "lib"
        LIBRARY DESTINATION "lib"
        RUNTIME DESTINATION "bin"
        PUBLIC_HEADER DESTINATION "include"
        COMPONENT dev
)

install(EXPORT LuaTargets
        FILE LuaTargets.cmake
        NAMESPACE Lua::
        DESTINATION "share/Lua"
)

export(PACKAGE lua)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/LuaConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
)

configure_file(LuaConfig.cmake.in
        "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/LuaConfig.cmake" @ONLY)

install(FILES
        "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/LuaConfig.cmake"
        "${PROJECT_BINARY_DIR}/LuaConfigVersion.cmake"
        DESTINATION "share/Lua"
)
