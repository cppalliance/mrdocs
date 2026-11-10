#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_detect_stdlib_tag(<out-definitions-var> <out-text-var>)
#
# Record the standard library tag of the compiler that builds MrDocs, the text
# of `MRDOCS_PLUGIN_STDLIB_TAG` in <mrdocs/Platform.hpp>.
#
# Example
#
#   mrdocs_detect_stdlib_tag(MRDOCS_STDLIB_TAG_DEFINITIONS MRDOCS_STDLIB_TAG)
#
# With GCC and libstdc++ on Linux (the release-linux preset), it sets
#
#   MRDOCS_STDLIB_TAG             = libstdc++, cxx11-abi 1, debug off
#   MRDOCS_STDLIB_TAG_DEFINITIONS = MRDOCS_EXPECTED_STDLIB_TAG="libstdc++, cxx11-abi 1, debug off"
#
# This is the shape the code produces, not a value measured in a CI log.
#
# mrdocs-config.cmake.in writes the text into the package as
# MRDOCS_BUILT_STDLIB_TAG. `mrdocs-plugin-api` (tools/mrdocs/CMakeLists.txt)
# gives the definitions to the plugins that link it, and
# mrdocs_add_plugin_module does the same for a plugin built with HOST. The
# first compile of such a plugin then checks its own tag (Plugin.hpp).
#
# The function compiles a small probe with the project's compiler and flags
# and reads the tag out of the object code. A probe is more reliable than
# a table of compiler versions: it reports whatever the library's own headers
# say, including settings that come from CMAKE_CXX_FLAGS such as
# `-D_GLIBCXX_USE_CXX11_ABI=0`. A generator with several configurations gets
# one probe for each, since the tag can differ between them.
#
# Parameters
#
#   <out-definitions-var>  The name of the variable that receives the compile
#                          definitions that give a plugin the tag, as
#                          MRDOCS_EXPECTED_STDLIB_TAG. It is empty for the MSVC
#                          standard library, whose tag depends on the
#                          configuration and the runtime flags of each target.
#   <out-text-var>         The name of the variable that receives the tag as
#                          text, with the configuration name in front of each
#                          one when they differ.
#
# Preconditions
#
#   The C++ language is enabled, and the headers of MrDocs are in
#   `include/` of the project's source directory.
#
# Guarantees
#
#   Both variables are set in the calling scope. When the probe does not
#   compile, or its object code does not hold the tag (a build with `-flto`
#   keeps bitcode in it), the function warns and leaves both variables empty,
#   so no plugin gets a tag to check and the load time check remains.
#
#   The probe sees CMAKE_CXX_FLAGS and its per-configuration variants, and the
#   COMPILE_DEFINITIONS and COMPILE_OPTIONS of the calling directory, without
#   those that hold a generator expression. A setting that reaches the
#   compiler any other way, such as a target property of MrDocs, is not seen.
function(mrdocs_detect_stdlib_tag out_definitions out_text)
    set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/mrdocs-stdlib-tag")
    # Only the object code is needed, so the probe is not linked.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    # A single-configuration generator with no CMAKE_BUILD_TYPE gets one probe
    # with the default flags, named by a placeholder that sets no configuration.
    set(configs ${CMAKE_CONFIGURATION_TYPES})
    if (NOT configs)
        set(configs "${CMAKE_BUILD_TYPE}")
    endif ()
    if (NOT configs)
        set(configs "default flags")
    endif ()
    # The definitions and options of the directory reach the probe too.
    set(probe_flags)
    get_directory_property(directory_definitions COMPILE_DEFINITIONS)
    foreach (item IN LISTS directory_definitions)
        if (NOT item MATCHES "\\$<")
            list(APPEND probe_flags "-D${item}")
        endif ()
    endforeach ()
    get_directory_property(directory_options COMPILE_OPTIONS)
    foreach (item IN LISTS directory_options)
        if (NOT item MATCHES "\\$<")
            string(REGEX REPLACE "^SHELL:" "" item "${item}")
            list(APPEND probe_flags "${item}")
        endif ()
    endforeach ()
    # Platform.hpp refuses a compiler whose __cplusplus stays at the MSVC
    # default.
    if (MSVC)
        list(APPEND probe_flags /Zc:__cplusplus)
    endif ()
    set(index 0)
    set(tags)
    foreach (config IN LISTS configs)
        if (NOT config STREQUAL "default flags")
            set(CMAKE_TRY_COMPILE_CONFIGURATION "${config}")
        endif ()
        math(EXPR index "${index} + 1")
        try_compile(mrdocs_stdlib_probe_compiled "${probe_dir}/build-${index}"
            SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StdlibTagProbe.cpp"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PROJECT_SOURCE_DIR}/include"
            COMPILE_DEFINITIONS ${probe_flags}
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            COPY_FILE "${probe_dir}/probe-${index}.bin"
            OUTPUT_VARIABLE output)
        set(compiled "${mrdocs_stdlib_probe_compiled}")
        unset(mrdocs_stdlib_probe_compiled CACHE)
        set(found)
        if (compiled)
            file(STRINGS "${probe_dir}/probe-${index}.bin" found
                REGEX "MRDOCS_STDLIB_TAG\\[[^]]*\\]" LIMIT_COUNT 1)
        endif ()
        if (NOT compiled OR NOT found MATCHES "MRDOCS_STDLIB_TAG\\[([^]]*)\\]")
            message(WARNING
                "The standard library probe gave no tag, so plugins are not "
                "checked at compile time, only when MrDocs loads them:\n${output}")
            set(${out_definitions} "" PARENT_SCOPE)
            set(${out_text} "" PARENT_SCOPE)
            return()
        endif ()
        list(APPEND tags "${CMAKE_MATCH_1}")
    endforeach ()
    set(distinct ${tags})
    list(REMOVE_DUPLICATES distinct)
    list(LENGTH distinct distinct_count)
    set(definitions)
    set(pieces)
    if (distinct_count EQUAL 1)
        set(pieces "${distinct}")
        set(definitions "MRDOCS_EXPECTED_STDLIB_TAG=\"${distinct}\"")
    else ()
        # The tag of each configuration is on its own condition.
        set(index 0)
        foreach (config IN LISTS configs)
            list(GET tags ${index} tag)
            math(EXPR index "${index} + 1")
            list(APPEND pieces "${config}: ${tag}")
            list(APPEND definitions
                "$<$<CONFIG:${config}>:MRDOCS_EXPECTED_STDLIB_TAG=\"${tag}\">")
        endforeach ()
    endif ()
    string(JOIN ", " text ${pieces})
    # With the MSVC standard library the tag also follows the runtime flags of
    # the target that compiles the plugin, which a probe does not see, so the
    # check is left to the load time.
    if (text MATCHES "msvc-stl")
        set(definitions)
    endif ()
    set(${out_definitions} "${definitions}" PARENT_SCOPE)
    set(${out_text} "${text}" PARENT_SCOPE)
endfunction()
