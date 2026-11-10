#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_target_link_whole_libraries(<target> <PUBLIC|PRIVATE|INTERFACE> <library>...)
#
# Link each <library> into <target> so that every object of it is in the final
# link, not only the ones something refers to.
#
# Examples
#
#   mrdocs_target_link_whole_libraries(mrdocs PRIVATE mrdocs-core)
#
# With a static core, `mrdocs` gets the plain private link and a link option
# for the archive, so a plugin can call functions that nothing in the tool calls.
# The link line of `mrdocs` has
#
#   Linux (GCC)   -Wl,--whole-archive,<src>/build/release-linux/src/libmrdocs-core.a,--no-whole-archive
#   macOS         -Xlinker -force_load -Xlinker /path/to/libmrdocs-core.a
#
# The Linux and macOS lines are the shape the code produces, not values
# measured in a CI log.
#
# With a shared core the call is `target_link_libraries(mrdocs PRIVATE mrdocs-core)`
# and nothing else.
#
#   mrdocs_target_link_whole_libraries(mrdocs-core-object-files PUBLIC
#       mrdocs-handlebars mrdocs-dom)
#
# A static library into an object library, as src/CMakeLists.txt does. The
# call links `mrdocs-handlebars` and not `mrdocs-dom`, which handlebars
# reaches already, and adds two INTERFACE link options, inside
# `$<BUILD_INTERFACE:...>`, one for each archive:
#
#   -Wl,--whole-archive,<src>/build/release-linux/libs/handlebars/libmrdocs-handlebars.a,--no-whole-archive
#   -Wl,--whole-archive,<src>/build/release-linux/libs/dom/libmrdocs-dom.a,--no-whole-archive
#
# Whatever links the object library, `mrdocs-core` and then `mrdocs`, loads
# every object of both archives.
#
# With the whole library linked, what it marks for export is there whether or not
# anything uses it. The function reads like `target_link_libraries`, and covers
# every mix of static and shared.
#
# Parameters
#
#   <target>   The library, object library or executable that links them.
#   PUBLIC|PRIVATE|INTERFACE
#              The scope of the link, as in `target_link_libraries`.
#   <library>  STATIC_LIBRARY or SHARED_LIBRARY targets.
#
# Preconditions
#
#   Every target exists. A library comes before the libraries it links in its
#   interface (`mrdocs-handlebars` before `mrdocs-dom`). In the other order the
#   function only adds a second link of the same library.
#
# Guarantees
#
#   | <library> | <target>                     | What the function does                         |
#   |-----------|------------------------------|------------------------------------------------|
#   | shared    | any                          | the plain link with the scope                  |
#   | static    | shared library or executable | the link with the scope, plus whole-archive    |
#   |           |                              | as a PRIVATE link option (INTERFACE with the   |
#   |           |                              | INTERFACE scope)                               |
#   | static    | static, object or interface  | the link with the scope, plus whole-archive as |
#   |           | library                      | an INTERFACE link option of <target>           |
#
#   - A shared library carries all its objects already.
#   - The whole-archive request of the platform is `/WHOLEARCHIVE:` with MSVC,
#     `-force_load` on Apple platforms and `--whole-archive` elsewhere, one per
#     archive.
#   - For a static, object or interface <target> the request is an INTERFACE link option,
#     because an archive cannot hold another archive and the final link has to
#     pull in the objects. It reaches whatever links <target>. It is left out
#     of an installed package, since it names a file of the build tree.
#   - The link is skipped for a <library> that <target> already reaches,
#     directly or through the interface of another library it links, because
#     a library that the linker sees twice is a warning on macOS.
function(mrdocs_target_link_whole_libraries target scope)
    if (NOT scope MATCHES "^(PUBLIC|PRIVATE|INTERFACE)$")
        message(FATAL_ERROR
            "mrdocs_target_link_whole_libraries(${target} ${scope} ...): the "
            "scope must be PUBLIC, PRIVATE or INTERFACE")
    endif ()
    get_target_property(target_type ${target} TYPE)
    if (target_type STREQUAL "STATIC_LIBRARY"
        OR target_type STREQUAL "OBJECT_LIBRARY"
        OR target_type STREQUAL "INTERFACE_LIBRARY")
        set(carries_objects TRUE)
    else ()
        set(carries_objects FALSE)
    endif ()
    foreach (library IN LISTS ARGN)
        get_target_property(library_type ${library} TYPE)
        if (library_type STREQUAL "SHARED_LIBRARY")
            target_link_libraries(${target} ${scope} ${library})
            continue ()
        endif ()
        _mrdocs_resolve_alias(library_name ${library})
        set(reachable FALSE)
        get_target_property(direct ${target} LINK_LIBRARIES)
        get_target_property(direct_interface ${target} INTERFACE_LINK_LIBRARIES)
        set(to_visit ${direct} ${direct_interface})
        set(visited "")
        while (to_visit)
            list(POP_FRONT to_visit item)
            string(REGEX REPLACE "^\\$<[A-Za-z_]+:(.+)>$" "\\1" item "${item}")
            if (NOT item OR NOT TARGET ${item})
                continue()
            endif ()
            _mrdocs_resolve_alias(item_name ${item})
            if (item_name STREQUAL library_name)
                set(reachable TRUE)
                break()
            endif ()
            if (item_name IN_LIST visited)
                continue()
            endif ()
            list(APPEND visited ${item_name})
            get_target_property(interface_libs ${item_name} INTERFACE_LINK_LIBRARIES)
            if (interface_libs)
                list(APPEND to_visit ${interface_libs})
            endif ()
        endwhile ()
        # The link relinks <target> when the library changes. It is left out
        # when <target> already reaches <library>.
        if (NOT reachable)
            target_link_libraries(${target} ${scope} ${library})
        endif ()
        if (carries_objects OR scope STREQUAL "INTERFACE")
            set(flag_scope INTERFACE)
            set(prefix "$<BUILD_INTERFACE:")
            set(suffix ">")
        else ()
            set(flag_scope PRIVATE)
            set(prefix "")
            set(suffix "")
        endif ()
        if (MSVC)
            target_link_options(${target} ${flag_scope}
                "${prefix}/WHOLEARCHIVE:$<TARGET_FILE:${library}>${suffix}")
        elseif (APPLE)
            target_link_options(${target} ${flag_scope}
                "${prefix}LINKER:-force_load,$<TARGET_FILE:${library}>${suffix}")
        else ()
            # One option per archive: CMake drops a repeated option of a
            # target, which would drop the second pair of --whole-archive flags.
            target_link_options(${target} ${flag_scope}
                "${prefix}LINKER:--whole-archive,$<TARGET_FILE:${library}>,--no-whole-archive${suffix}")
        endif ()
    endforeach ()
endfunction()

# _mrdocs_resolve_alias(<out-var> <target>)
#
# Set <out-var> to the real target behind <target>, which is <target> itself
# unless it is an ALIAS (`mrdocs::dom` resolves to `mrdocs-dom`). Used by
# mrdocs_target_link_whole_libraries to compare link items by identity.
function(_mrdocs_resolve_alias out_var target)
    get_target_property(aliased ${target} ALIASED_TARGET)
    if (aliased)
        set(${out_var} ${aliased} PARENT_SCOPE)
    else ()
        set(${out_var} ${target} PARENT_SCOPE)
    endif ()
endfunction()
