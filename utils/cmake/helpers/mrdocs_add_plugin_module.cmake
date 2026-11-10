#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_add_plugin_module(<target> <source>... [NAME <name>] [ADDONS <addon-root>] [FLAT] [HOST <target>])
#
# mrdocs_add_plugin, which the installed package provides, plus HOST: a host
# executable the library links in place of `mrdocs::plugin-api`.
#
# Example
#
#   mrdocs_add_plugin_module(mrdocs-plugin-reload reload/ReloadPlugin.cpp
#       ADDONS "${MRDOCS_RELOAD_ADDONS}" FLAT HOST mrdocs-plugin-reload-host)
#
# In the build directory of tests/integration/plugin-api, it writes
#
#   reload-addons/extensions/mrdocs-plugin-reload.so
#
# and sets on the target
#
#   LINK_LIBRARIES      = mrdocs-plugin-reload-host;mrdocs-core-headers
#   compile definitions   MRDOCS_DYNAMIC_LINK
#                         MRDOCS_EXPECTED_STDLIB_TAG="libstdc++, cxx11-abi 1, debug off"
#
# The test then runs the host on that addon root:
#
#   add_test(NAME mrdocs-plugin-reload
#       COMMAND mrdocs-plugin-reload-host "${MRDOCS_RELOAD_ADDONS}")
#
# Parameters
#
#   <target>, <source>..., NAME, ADDONS, FLAT
#       As in mrdocs_add_plugin, and the library is written the same way. The
#       defaults are the same too: NAME is <target>, there is no addon root
#       without ADDONS, and FLAT is off.
#   HOST <target>
#       A host made with mrdocs_target_plugin_host_compile_definitions.
#       The plugin links it, plus the headers and, when the core is shared,
#       `mrdocs-core`, in place of `mrdocs::plugin-api`. Without HOST the
#       plugin links `mrdocs::plugin-api`, as mrdocs_add_plugin does.
#
# Preconditions
#
#   The host exists when HOST is given. The function belongs to the MrDocs
#   build and is not part of the installed package.
#
# Guarantees
#
#   The link list of the library is the host and what a plugin needs of MrDocs
#   besides it, and nothing of `mrdocs::plugin-api`.
function(mrdocs_add_plugin_module target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "HOST" "")
    mrdocs_add_plugin(${target} ${arg_UNPARSED_ARGUMENTS})
    if (arg_HOST)
        # mrdocs_add_plugin links `mrdocs::plugin-api`, and target_link_libraries
        # cannot remove it, so the link list is replaced by the host, with
        # what `mrdocs::plugin-api` adds besides the executable.
        set_property(TARGET ${target} PROPERTY LINK_LIBRARIES ${arg_HOST} mrdocs-core-headers)
        target_compile_definitions(${target} PRIVATE MRDOCS_DYNAMIC_LINK)
        # The standard library tag MrDocs was built with, as mrdocs::plugin-api
        # gives it.
        if (MRDOCS_STDLIB_TAG_DEFINITIONS)
            target_compile_definitions(${target} PRIVATE
                ${MRDOCS_STDLIB_TAG_DEFINITIONS})
        endif ()
        if (MRDOCS_BUILD_SHARED)
            target_link_libraries(${target} PRIVATE mrdocs-core)
        endif ()
    endif ()
endfunction()
