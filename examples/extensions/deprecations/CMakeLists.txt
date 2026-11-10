#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Built the way count-by-kind/generator is: with mrdocs_add_plugin, in a
# directory of its own under the extensions subdirectory of an addon root of
# its own.
set(MRDOCS_DEPRECATIONS_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons")
mrdocs_add_plugin(mrdocs-deprecations-plugin-example plugin.cpp
    NAME deprecations ADDONS "${MRDOCS_DEPRECATIONS_ADDONS}")

# Documenting the sample project runs the generator and the transform.
add_test(NAME mrdocs-plugin-example-deprecations
    COMMAND
        mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/sample-project/mrdocs.yml"
        "--output=${CMAKE_CURRENT_BINARY_DIR}/reference-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${MRDOCS_DEPRECATIONS_ADDONS}"
        --log-level=warn
)
set_tests_properties(mrdocs-plugin-example-deprecations PROPERTIES
    FIXTURES_SETUP mrdocs-plugin-example-deprecations-output)
# This test compares the report with the file the documentation shows.
add_test(NAME mrdocs-plugin-example-deprecations-compare
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${CMAKE_CURRENT_BINARY_DIR}/reference-output/deprecations.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/sample-project/deprecations.txt"
)
set_tests_properties(mrdocs-plugin-example-deprecations-compare PROPERTIES
    FIXTURES_REQUIRED mrdocs-plugin-example-deprecations-output)

# The transform changes the corpus every generator sees, so the XML
# generator, run next to the plugin, shows the briefs it wrote.
add_test(NAME mrdocs-plugin-example-deprecation-brief
    COMMAND
        mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/sample-project/mrdocs.yml"
        --generator=xml
        "--output=${CMAKE_CURRENT_BINARY_DIR}/transform-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${MRDOCS_DEPRECATIONS_ADDONS}"
        --log-level=warn
)
set_tests_properties(mrdocs-plugin-example-deprecation-brief PROPERTIES
    FIXTURES_SETUP mrdocs-plugin-example-deprecation-brief-output)
add_test(NAME mrdocs-plugin-example-deprecation-brief-compare
    COMMAND ${CMAKE_COMMAND}
        "-DXML=${CMAKE_CURRENT_BINARY_DIR}/transform-output/reference.xml"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check-brief.cmake"
)
set_tests_properties(mrdocs-plugin-example-deprecation-brief-compare PROPERTIES
    FIXTURES_REQUIRED mrdocs-plugin-example-deprecation-brief-output)
