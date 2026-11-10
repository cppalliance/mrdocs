#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_add_hidden_archives(<name> <dependency>...)
#
# Create an INTERFACE target <name> that links the static archives behind the
# dependencies without exporting their symbols, so the target that links <name>
# exposes only the MrDocs API (what MRDOCS_DECL marks) to plugins.
#
# Example
#
#   mrdocs_add_hidden_archives(mrdocs-hidden-archives
#       mrdocs-llvm jerryscript::jerry-core jerryscript::jerry-port Lua::lua)
#
# The archives behind those four dependencies are the static targets they link,
# directly or not. The call creates the INTERFACE target and sets
#
#   INTERFACE_MRDOCS_HIDDEN_ARCHIVES =
#       jerryscript::jerry-core;jerryscript::jerry-port;Lua::lua;clangAST;...;LLVMSupport;...
#
# For `Lua::lua` at `/opt/lua/lib/liblua.a`, the target adds the link option
#
#   Linux (GCC)   LINKER:--exclude-libs,$<TARGET_FILE_NAME:Lua::lua>
#                 -Wl,--exclude-libs,liblua.a
#   macOS         LINKER:-load_hidden,$<TARGET_FILE:Lua::lua>
#                 -Xlinker -load_hidden -Xlinker /opt/lua/lib/liblua.a
#   MSVC          (none)
#
# and one such pair for each archive. The Linux and macOS lines are the shape
# the code produces, not values measured in a CI log. src/CMakeLists.txt then
# links the target to the core, so the `mrdocs` link line has them and the
# executable does not export what they define:
#
#   target_link_libraries(mrdocs-core PRIVATE
#       $<BUILD_INTERFACE:mrdocs-hidden-archives>)
#
# With a dependency that is a project STATIC_LIBRARY instead of an imported
# target, the configure stops with an error that names it.
#
# LLVM, Clang, Lua and JerryScript are built with default visibility, and a
# plugin that bundles its own copy of one of them has to bind to its own copy.
#
# Parameters
#
#   <name>          The name of the INTERFACE target to create.
#   <dependency>... The targets whose static archives are hidden. Aliases,
#                   imported targets and `$<BUILD_INTERFACE:target>` items are
#                   accepted, as in mrdocs_collect_static_libs.
#
# Preconditions
#
#   Every dependency is visible in the calling directory, and every static
#   archive behind them is an imported target, because the function makes
#   each one global (IMPORTED_GLOBAL) so that the link options resolve in the
#   directories that link <name>. A static archive that is not imported ends
#   the configuration with an error.
#
# Guarantees
#
#   - The dependency graph is scanned when the function runs. A target that
#     links <name> scans nothing.
#   - <name> carries one link option per archive: `-load_hidden` on Apple
#     platforms, `--exclude-libs` elsewhere, and nothing with MSVC, which
#     exports only what is marked.
#   - The archives are stored, as target names, in the property
#     INTERFACE_MRDOCS_HIDDEN_ARCHIVES of <name>. It has the INTERFACE_ prefix
#     because CMake before 3.19 only accepts such properties on an INTERFACE
#     library.
#   - The function creates no link between <name> and anything else. A target
#     that needs the options links <name> itself.
function(mrdocs_add_hidden_archives name)
    mrdocs_collect_static_libs(archive_files system_libs ${ARGN})
    set(archives "")
    foreach (archive IN LISTS archive_files)
        string(REGEX REPLACE "^\\$<TARGET_FILE:(.*)>$" "\\1" archive "${archive}")
        get_target_property(imported ${archive} IMPORTED)
        if (NOT imported)
            message(FATAL_ERROR
                "${archive} is a static library linked into ${name} that is "
                "not imported, so it cannot be linked with hidden symbols")
        endif ()
        # A generator expression naming an imported target only resolves where
        # the target is visible, so each one is made global. They are imported
        # in the calling directory, which is where the property may be set.
        set_target_properties(${archive} PROPERTIES IMPORTED_GLOBAL TRUE)
        list(APPEND archives ${archive})
    endforeach ()
    add_library(${name} INTERFACE)
    set_target_properties(${name} PROPERTIES
        INTERFACE_MRDOCS_HIDDEN_ARCHIVES "${archives}")
    if (MSVC)
        return()
    endif ()
    foreach (archive IN LISTS archives)
        if (APPLE)
            target_link_options(${name} INTERFACE
                "LINKER:-load_hidden,$<TARGET_FILE:${archive}>")
        else ()
            target_link_options(${name} INTERFACE
                "LINKER:--exclude-libs,$<TARGET_FILE_NAME:${archive}>")
        endif ()
    endforeach ()
endfunction()
