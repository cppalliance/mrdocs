#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# mrdocs_add_plugin(<target> <source>... [NAME <name>] [ADDONS <addon-root>] [FLAT])
#
# Create a module library named <target> from the <source> files, for MrDocs to
# load as a plugin when it starts up.
#
# Examples
#
#   The plain call builds the plugin `foo` from `foo.cpp`:
#
#     mrdocs_add_plugin(foo foo.cpp)
#
#   The library goes where CMake puts module libraries, by default in the build
#   directory of the calling CMakeLists.txt (a subdirectory of it for each
#   configuration with Visual Studio or Xcode). The call writes `libfoo.so` on
#   Linux and macOS, and `foo.dll` on Windows.
#
#   With ADDONS, the library goes to an addon root, where MrDocs looks for it:
#
#     mrdocs_add_plugin(foo foo.cpp ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons")
#
#   The call writes the library to `addons/extensions/foo/` in the build
#   directory, with the file name of each platform:
#
#     Linux    libfoo.so
#     macOS    libfoo.so
#     Windows  foo.dll
#
#   Then `mrdocs --addons-supplemental=<build>/addons` loads the
#   extension named `foo`.
#
#   With NAME, the extension has a name of its own. A CMake target name must be
#   unique in the whole build, so a plugin target often carries a prefix, such
#   as `mrdocs-stats-plugin-example`. The extension name is what users type in
#   mrdocs.yml, such as `stats`:
#
#     mrdocs_add_plugin(mrdocs-stats-plugin-example plugin.cpp
#         NAME stats ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons")
#
#   The call writes `addons/extensions/stats/libstats.so`, or `stats.dll` on
#   Windows, and `generator: stats` in mrdocs.yml loads it.
#
#   With FLAT, the library has no directory of its own and no `lib` prefix:
#
#     mrdocs_add_plugin(mrdocs-stats-plugin-example plugin.cpp
#         NAME stats ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons" FLAT)
#
#   The call writes `addons/extensions/stats.so`, or `stats.dll` on Windows.
#
# Parameters
#
#   <target>     The name of the library target to create.
#   <source>...  The source files of the plugin.
#   NAME <name>  The name the extension loads under. It is the OUTPUT_NAME of
#                the library, so the file is `lib<name>.so`
#                (`<name>.dll` on Windows). With ADDONS it sits in a directory
#                named <name>, or with FLAT it is `<name>.so` directly in
#                `extensions/`. It defaults to <target>.
#   ADDONS <addon-root>
#                An addon root. The library is written to
#                `<addon-root>/extensions/<name>/`, where MrDocs looks for it.
#                Without ADDONS the library goes where CMake puts module
#                libraries. Run `mrdocs --addons-supplemental=<addon-root>` to
#                load it. <addon-root> is normally a directory in the build
#                tree, and a relative path is resolved against
#                CMAKE_CURRENT_BINARY_DIR. An absolute path is used as it is.
#                To put a plugin into an addon root outside the build tree, use
#                install(TARGETS). ADDONS has no default: when it is left out,
#                the library is not written to an addon root.
#   FLAT         Write the library to `<addon-root>/extensions/` itself,
#                without the `lib` prefix, because MrDocs accepts that prefix
#                only inside a directory. FLAT is off by default, so with
#                ADDONS the library goes to `<addon-root>/extensions/<name>/`
#                and keeps the `lib` prefix. Without ADDONS, FLAT has no effect.
#
# Preconditions
#
#   Call find_package(mrdocs REQUIRED) first, so that `mrdocs::plugin-api`
#   exists. In the MrDocs build it is an alias of `mrdocs-plugin-api`.
#
# Guarantees
#
#   The library links `mrdocs::plugin-api`, is compiled with hidden visibility,
#   and finds the shared libraries that sit next to it.
#
#   The function refuses a plugin that does not match the MrDocs it links, with
#   one check per mismatch. The compiler is not checked. A plugin that gets past
#   these checks is checked again when it loads.
#
#   - Always, at configure: `mrdocs::plugin-api` exists, so find_package(mrdocs)
#     ran. The message starts with
#     "mrdocs_add_plugin(foo): no mrdocs::plugin-api target."
#   - Every platform, at the first compile: the standard library tag of the
#     plugin equals the one of MrDocs. `mrdocs::plugin-api` defines
#     MRDOCS_EXPECTED_STDLIB_TAG, and a static_assert in
#     include/mrdocs/Plugin.hpp stops the compile when the plugin's own tag
#     differs. An example is a plugin built with `-D_GLIBCXX_USE_CXX11_ABI=0`
#     against a MrDocs built without it. The message starts with "the standard
#     library settings of this plugin differ from the ones MrDocs was built
#     with". With the MSVC standard library only the load checks it, and the
#     check is absent when the probe gave no tag, for example under -flto.
#   - macOS, at configure: the deployment target is not newer than the one
#     MrDocs was built with. The target is CMAKE_OSX_DEPLOYMENT_TARGET, or the
#     version of the host when that is unset. The check is skipped in the
#     MrDocs build itself and when MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET is empty.
#     The message starts with
#     "mrdocs_add_plugin(foo): the deployment target is 15.0".
#   - MSVC, at configure: the runtime is the DLL one (/MD), as MrDocs's. Only
#     CMAKE_MSVC_RUNTIME_LIBRARY is checked. The message starts with
#     "mrdocs_add_plugin(foo): the plugin's runtime is".
#
#   Linux has no check of its own beyond the standard library tag.
function(mrdocs_add_plugin target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "FLAT" "NAME;ADDONS" "")
    add_library(${target} MODULE ${arg_UNPARSED_ARGUMENTS})
    if (NOT arg_NAME)
        set(arg_NAME ${target})
    endif ()
    if (NOT TARGET mrdocs::plugin-api)
        message(FATAL_ERROR
            "mrdocs_add_plugin(${target}): no mrdocs::plugin-api target. "
            "Call find_package(mrdocs REQUIRED) first.")
    endif ()
    # Gives the plugin the include directories and the language level of the
    # public headers, and the symbols of the MrDocs API it calls.
    target_link_libraries(${target} PRIVATE mrdocs::plugin-api)
    # On Apple platforms, the deployment target must not be newer than the one
    # MrDocs was built with (MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET; an empty
    # exported value means no check). In the MrDocs build the variable does not
    # exist, so nothing is checked.
    if (APPLE AND MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET)
        # The target is CMAKE_OSX_DEPLOYMENT_TARGET, or the version of the host
        # when that is unset, because the compiler then builds for the host.
        set(effective_target "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        if (NOT effective_target)
            execute_process(COMMAND sw_vers -productVersion
                OUTPUT_VARIABLE effective_target
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        endif ()
        if (effective_target AND
            effective_target VERSION_GREATER MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET)
            message(FATAL_ERROR
                "mrdocs_add_plugin(${target}): the deployment target is "
                "${effective_target} (CMAKE_OSX_DEPLOYMENT_TARGET, or the host "
                "version when it is unset), newer than the "
                "${MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET} Mr.Docs was built with. "
                "Set CMAKE_OSX_DEPLOYMENT_TARGET to "
                "${MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET} or lower before project().")
        endif ()
    endif ()
    # MrDocs refuses a plugin that does not use the DLL runtime. Only
    # CMAKE_MSVC_RUNTIME_LIBRARY is checked: a MSVC_RUNTIME_LIBRARY property set
    # on the target after the call is not.
    if (MSVC)
        set(runtime "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        if (runtime MATCHES "MultiThreaded" AND NOT runtime MATCHES "DLL")
            message(FATAL_ERROR
                "mrdocs_add_plugin(${target}): the plugin's runtime is "
                "\"${runtime}\", the static one, and Mr.Docs was built with "
                "\"${MRDOCS_BUILT_MSVC_RUNTIME_LIBRARY}\" (empty means the DLL "
                "runtime, the default). Set CMAKE_MSVC_RUNTIME_LIBRARY to "
                "\"MultiThreaded$<$<CONFIG:Debug>:Debug>DLL\" (/MD).")
        endif ()
    endif ()
    # Hidden visibility, so what the library exports is what MRDOCS_PLUGIN_MAIN
    # marks.
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME ${arg_NAME}
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        BUILD_WITH_INSTALL_RPATH ON)
    # The run path is the directory the library is in, so a plugin finds the
    # shared libraries that sit next to it.
    if (APPLE)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path")
    elseif (UNIX)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif ()
    if (arg_ADDONS)
        # A relative addon root is in the build tree.
        if (NOT IS_ABSOLUTE "${arg_ADDONS}" AND NOT arg_ADDONS MATCHES "^\\$<")
            get_filename_component(arg_ADDONS "${arg_ADDONS}"
                ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
        endif ()
        set(directory "${arg_ADDONS}/extensions")
        if (NOT arg_FLAT)
            set(directory "${directory}/${arg_NAME}")
        endif ()
        # $<1:...> keeps CMake from appending a per-configuration subdirectory.
        set_target_properties(${target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "$<1:${directory}>")
        if (arg_FLAT)
            # MrDocs accepts the `lib` prefix only inside a directory of the
            # extension's own, so a file directly in `extensions/` is named
            # <name>.so.
            set_target_properties(${target} PROPERTIES PREFIX "")
        endif ()
    endif ()
endfunction()
