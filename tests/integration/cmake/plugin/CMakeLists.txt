#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Builds a plugin with the helper the package ships and loads it into the tool,
# as a plugin author does. The tests document the example project of
# ../executable.

set(plugin_addons "${CMAKE_CURRENT_BINARY_DIR}/plugin-addons")
set(example_dir "${CMAKE_CURRENT_SOURCE_DIR}/../executable")
# The plugin is built with the function the package ships, written to the
# extensions subdirectory of an addon root the test can name. The consumer is
# built against the installed package of the MrDocs it runs under, so the plugin
# carries that build's identity and the standard library the package was
# configured with.
mrdocs_add_plugin(consumer_plugin plugin.cpp
    ADDONS "${plugin_addons}" FLAT)

# Runs the `consumer-probe` generator the plugin installs.
add_test(NAME consumer-plugin-loads
    COMMAND ${MRDOCS_EXECUTABLE}
        --config=${example_dir}/mrdocs.yml
        --compilation-database=${example_dir}/docs/compile_commands.json
        --output=${CMAKE_CURRENT_BINARY_DIR}/plugin-reference
        --addons-supplemental=${plugin_addons}
        --generator=consumer-probe)

# Guards the macOS deployment target check.
if (APPLE)
    # The package writes the target the build used as
    # MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET in mrdocs-config.cmake, and
    # mrdocs_add_plugin reads that same name. Renaming either side leaves the
    # check silently off, so this test configures a copy of the project with a
    # deployment target above the built one and expects the helper's refusal.
    set(osx_refusal_dir "${CMAKE_CURRENT_BINARY_DIR}/osx-target-refusal")
    add_test(NAME consumer-osx-deployment-target-refusal
        COMMAND ${CMAKE_COMMAND}
            -S ${PROJECT_SOURCE_DIR}
            -B ${osx_refusal_dir}
            -G ${CMAKE_GENERATOR}
            -D CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -D CMAKE_OSX_DEPLOYMENT_TARGET=99.0
            -D mrdocs_DIR=${mrdocs_DIR})
    set_tests_properties(consumer-osx-deployment-target-refusal PROPERTIES
        PASS_REGULAR_EXPRESSION
            "mrdocs_add_plugin\\(consumer_plugin\\): the deployment target is 99\\.0")
    # A config that does not define the name stops the configure.
    if (NOT DEFINED MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET)
        message(FATAL_ERROR
            "mrdocs-config.cmake does not set MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET, the name mrdocs_add_plugin reads")
    elseif (MRDOCS_BUILT_OSX_DEPLOYMENT_TARGET STREQUAL "")
        # A package built at the host's default macOS exports an empty value,
        # which leaves nothing to compare against: the test is registered but
        # disabled.
        set_tests_properties(consumer-osx-deployment-target-refusal PROPERTIES
            DISABLED TRUE)
    endif ()
endif ()

get_property(consumer_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${consumer_tests} PROPERTIES LABELS integration)
