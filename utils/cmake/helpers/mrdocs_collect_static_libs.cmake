#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_collect_static_libs(<out-static-var> <out-system-var> <target>...)
#
# Collect the static libraries and the system libraries the given targets link,
# directly or transitively through the libraries they link.
#
# Example
#
#   mrdocs_collect_static_libs(_mrdocs_bundle_deps _mrdocs_bundle_system_libs
#       mrdocs-llvm jerryscript::jerry-core jerryscript::jerry-port Lua::lua)
#
# On macOS, it sets
#
#   _mrdocs_bundle_deps = $<TARGET_FILE:jerryscript::jerry-core>;...;$<TARGET_FILE:LLVMSupport>;...
#   _mrdocs_bundle_system_libs = m
#
# src/CMakeLists.txt then merges the archives into `libmrdocs-core-bundled.a`,
# and links the system libraries to the installed core:
#
#   if (APPLE)
#       set(_bundle_inputs
#               -Wl,-force_load,$<TARGET_FILE:mrdocs-core>
#               ${_mrdocs_bundle_deps})
#   ...
#   add_custom_command(
#           ...
#           COMMAND ${CMAKE_CXX_COMPILER} -r -nostdlib -o ${_mrdocs_bundled_obj_path} ${_bundle_inputs}
#   target_link_libraries(mrdocs-core INTERFACE
#       "$<INSTALL_INTERFACE:${_mrdocs_bundle_system_libs}>")
#
# Parameters
#
#   <out-static-var>  The name of the variable that receives the static-library
#                     files, as `$<TARGET_FILE:...>` expressions, for merging
#                     into a bundle.
#   <out-system-var>  The name of the variable that receives the OS and system
#                     libraries (pthread, ntdll, ...), named as non-targets,
#                     which cannot be bundled and must be relinked.
#   <target>...       The targets to start from. Aliases, imported targets and
#                     `$<BUILD_INTERFACE:target>` items are accepted.
#
# Preconditions
#
#   Every target named has to be visible in the calling directory.
#
# Guarantees
#
#   It is a breadth-first walk that visits each target once, so a link cycle
#   ends. Both variables are set in the calling scope, without duplicates, and
#   are empty when nothing was found.
function(mrdocs_collect_static_libs out_var out_system_var)
    # Breadth-first walk of the link graph.
    set(_static_libs "")        # the static-library files found, as $<TARGET_FILE:>
    set(_system_libs "")        # non-target link items (system libraries / flags)
    set(_visited "")            # targets already processed (guards against cycles)
    set(_to_visit "${ARGN}")    # worklist of targets still to process

    while(_to_visit)
        list(POP_FRONT _to_visit _entry)

        # Peel that generator expressions (e.g.: $<BUILD_INTERFACE:mrdocs-llvm> -> mrdocs-llvm)
        string(REGEX REPLACE "^\\$<[A-Za-z_]+:(.+)>$" "\\1" _entry "${_entry}")

        # A non-target entry is a system library (pthread, ntdll, ...) or a raw
        # linker flag: it can't be bundled so we return it so it can be remembered
        # and relinked at install time later.
        # When the toolchain is bundled the imported targets that would otherwise
        # carry these are dropped, so a consumer needs them on its own link line.
        if (NOT _entry OR NOT TARGET ${_entry})
            # Genex fragments left by an incomplete peel are not libraries: skip those.
            if(_entry AND NOT _entry MATCHES "[$<>]")
                list(APPEND _system_libs "${_entry}")
            endif()
            continue()
        endif()

        # Resolve an alias to the target it stands for: (e.g. mrdocs::dom -> mrdocs-dom)
        get_target_property(_aliased ${_entry} ALIASED_TARGET)
        if(_aliased)
            set(_entry ${_aliased})
        endif()

        # Process each target once in graph diamonds
        if(_entry IN_LIST _visited)
            continue()
        endif()
        list(APPEND _visited ${_entry})

        # Store this node in the result when it is a static archive we can merge.
        get_target_property(_type ${_entry} TYPE)
        if(_type STREQUAL "STATIC_LIBRARY")
            list(APPEND _static_libs "$<TARGET_FILE:${_entry}>")
        elseif(_type STREQUAL "UNKNOWN_LIBRARY")
            # A "Find" module located the file by path (find_library)
            # Merge it ONLY if that file is actually a static
            set(_loc "")
            get_target_property(_configs ${_entry} IMPORTED_CONFIGURATIONS)
            if(_configs)
                list(GET _configs 0 _cfg)
                get_target_property(_loc ${_entry} IMPORTED_LOCATION_${_cfg})
            endif()
            if(NOT _loc)
                get_target_property(_loc ${_entry} IMPORTED_LOCATION)
            endif()
            string(REPLACE "." "\\." _static_suffix "${CMAKE_STATIC_LIBRARY_SUFFIX}")
            if(_loc AND _loc MATCHES "${_static_suffix}$")
                list(APPEND _static_libs "$<TARGET_FILE:${_entry}>")
            endif()
        endif()

        # Queue this target's public and private linked libraries
        get_target_property(_interface_libs ${_entry} INTERFACE_LINK_LIBRARIES)
        if(_interface_libs)
            list(APPEND _to_visit ${_interface_libs})
        endif()
        if(NOT _type STREQUAL "INTERFACE_LIBRARY")
            get_target_property(_private_libs ${_entry} LINK_LIBRARIES)
            if(_private_libs)
                list(APPEND _to_visit ${_private_libs})
            endif()
        endif()
    endwhile()

    if(_static_libs)
        list(REMOVE_DUPLICATES _static_libs)
    endif()
    if(_system_libs)
        list(REMOVE_DUPLICATES _system_libs)
    endif()
    set(${out_var} "${_static_libs}" PARENT_SCOPE)
    set(${out_system_var} "${_system_libs}" PARENT_SCOPE)
endfunction()
