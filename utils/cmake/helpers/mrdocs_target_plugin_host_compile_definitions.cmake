#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_target_plugin_host_compile_definitions(<target>)
#
# Turn an executable into a plugin host: a program that loads MrDocs plugins
# and gives them the MrDocs API to call.
#
# Example
#
#   add_executable(mrdocs ${TOOL_SOURCES})
#   mrdocs_target_plugin_host_compile_definitions(mrdocs)
#
# With a static core, it sets on `mrdocs`
#
#   ENABLE_EXPORTS        = ON
#   CXX_VISIBILITY_PRESET = hidden
#   compile definition    MRDOCS_SOURCE
#
# and links `mrdocs-core` whole (the `--whole-archive` line of
# mrdocs_target_link_whole_libraries). tools/mrdocs/CMakeLists.txt then makes the
# target that plugins link point at the host, so a plugin resolves the API
# against the executable:
#
#   target_link_libraries(mrdocs-plugin-api INTERFACE mrdocs)
#
# tests/integration/plugin-api makes a second host the same way, for the plugin
# of its reload test (see mrdocs_add_plugin_module).
#
# Parameters
#
#   <target>  An executable target.
#
# Preconditions
#
#   The executable exists already (add_executable) and links nothing of
#   MrDocs itself. It does not link LLVM either: what it needs from it, such as
#   crash handlers, goes through the functions of <mrdocs/Support/Process.hpp>,
#   so that a process has one copy of LLVM, the one in `mrdocs-core`. The
#   function belongs to the MrDocs build and is not part of the installed
#   package.
#
# Guarantees
#
#   - The host links `mrdocs-core` privately, and links every object of it when
#     the core is static.
#   - The host is compiled with hidden visibility and has ENABLE_EXPORTS.
#   - A plugin that links `mrdocs::plugin-api` compiles and links against the
#     host: with a static core the host provides the API and the plugin leaves
#     what it calls undefined. With a shared core the plugin links
#     `mrdocs-core` itself, and the host shares that copy.
#   - With a static core, the host defines MRDOCS_SOURCE. It also gets
#     `mrdocs-hidden-archives` from `mrdocs-core`, which links it PRIVATE
#     (passed on under CMP0099), so the host does not export the symbols of
#     the archives of LLVM, Clang, Lua and JerryScript.
#   - With a shared core on Windows, the host exports one function of no use
#     and gets mrdocs_ensure_runtime_dlls.
#   - It does not install the host or add it to an export set (the `mrdocs`
#     tool installs itself in tools/mrdocs/CMakeLists.txt), and it does not
#     build plugins, which mrdocs_add_plugin does.
function(mrdocs_target_plugin_host_compile_definitions target)
    target_include_directories(${target}
        PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}/src")
    # What the host exports is what MRDOCS_DECL marks.
    mrdocs_hide_symbols(${target})
    # CMake requires it before it lets a module library link an executable at
    # all. With a static core the plugin leaves what it calls undefined: the
    # dynamic loader resolves it against the host on Linux, CMake links the host
    # as the bundle loader on macOS, and the import library the host writes is
    # linked on Windows.
    set_target_properties(${target} PROPERTIES ENABLE_EXPORTS ON)
    # Whole archive when the core is static, a plain private link when shared. A
    # plugin calls functions nothing in the host calls, such as `assertFailed`
    # in a Release build.
    mrdocs_target_link_whole_libraries(${target} PRIVATE mrdocs-core)
    if (MRDOCS_BUILD_SHARED)
        if (WIN32)
            # MRDOCS_DECL is an import here, so the host has no export of its
            # own, and an executable that exports nothing gets no import library.
            # This gives it one (`mrdocs_plugin_host_export`), so the linker
            # writes the import library a plugin links against.
            target_sources(${target} PRIVATE
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/HostExport.cpp")
        endif ()
        # Windows finds mrdocs-core.dll only next to the program or on PATH.
        mrdocs_ensure_runtime_dlls(${target})
    else ()
        # Makes MRDOCS_DECL mean export in the host, which carries the API.
        target_compile_definitions(${target} PRIVATE MRDOCS_SOURCE)
    endif ()
endfunction()
