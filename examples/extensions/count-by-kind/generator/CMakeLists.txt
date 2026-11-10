#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# MrDocs looks for extensions in the extensions subdirectory of an addon root.
set(MRDOCS_STATS_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons")
# The library is built into a directory of its own there, named `stats`, the
# name the extension loads under. In the MrDocs build the plugin links
# `mrdocs::plugin-api`, which leads to the tool that was just built. Out of
# tree, a plugin author links the installed one, as the Defining Extensions page
# shows.
mrdocs_add_plugin(mrdocs-stats-plugin-example plugin.cpp
    NAME stats ADDONS "${MRDOCS_STATS_ADDONS}")

# Documenting the sample project is the test: its configuration asks for
# the `stats` generator, which exists only if the plugin was found,
# loaded, and installed it.
add_test(NAME mrdocs-plugin-example-stats
    COMMAND
        mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/sample-project/mrdocs.yml"
        "--output=${CMAKE_CURRENT_BINARY_DIR}/reference-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${MRDOCS_STATS_ADDONS}"
        --log-level=warn
)
set_tests_properties(mrdocs-plugin-example-stats PROPERTIES
    FIXTURES_SETUP mrdocs-plugin-example-stats)

# The output of the generator has to match the stats.txt the documentation
# shows. Line endings are ignored, since the generator writes the
# platform's own and a checkout may convert the committed file.
add_test(NAME mrdocs-plugin-example-stats-output
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol
        "${CMAKE_CURRENT_BINARY_DIR}/reference-output/stats.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/sample-project/stats.txt")
set_tests_properties(mrdocs-plugin-example-stats-output PROPERTIES
    FIXTURES_REQUIRED mrdocs-plugin-example-stats)
