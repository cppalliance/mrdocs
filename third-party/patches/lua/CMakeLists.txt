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

# Check if ./src exists
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/src")
    # Lua sources live under ./src
    file(GLOB LUA_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
    list(REMOVE_ITEM LUA_SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/src/lua.c"
            "${CMAKE_CURRENT_LIST_DIR}/src/luac.c"
    )
    file(GLOB LUA_HEADERS "${CMAKE_CURRENT_LIST_DIR}/src/*.h")
else()
    # Lua sources live under ./ (github release)
    file(GLOB LUA_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.c")
    list(REMOVE_ITEM LUA_SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/lua.c"
            "${CMAKE_CURRENT_LIST_DIR}/luac.c"
    )
    file(GLOB LUA_HEADERS "${CMAKE_CURRENT_LIST_DIR}/*.h")
endif()

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
