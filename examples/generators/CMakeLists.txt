#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Generator examples exercised as run-only tests: the test only checks that
# mrdocs runs each example to completion using the example's own
# configuration. Output goes to the build tree; the committed output files are
# illustrative and are not compared here.

# Data-driven generators: examples/generators/data-driven/<id> demonstrates a
# Handlebars generator defined entirely in an addon.
foreach (data_driven IN ITEMS md tex jsonl)
    add_test(NAME mrdocs-generator-example-data-driven-${data_driven}
        COMMAND
            mrdocs
            "${CMAKE_CURRENT_SOURCE_DIR}/data-driven/${data_driven}/simple.cpp"
            "--config=${CMAKE_CURRENT_SOURCE_DIR}/data-driven/${data_driven}/mrdocs.yml"
            "--output=${CMAKE_CURRENT_BINARY_DIR}/data-driven/${data_driven}"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            --log-level=warn
    )
endforeach ()

# Script-driven generator examples (a shell driver that invokes mrdocs).
foreach (script_driven IN ITEMS search-index json toc describe snippets)
    add_test(
        NAME mrdocs-generator-script-driven-${script_driven}
        COMMAND bash run.sh
                --output=${CMAKE_CURRENT_BINARY_DIR}/script-driven/${script_driven}/reference-output
                ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/script-driven/${script_driven}
    )
    set_property(TEST mrdocs-generator-script-driven-${script_driven} PROPERTY
        ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")
endforeach ()

# No-op generator examples (the documentation check and the AI repair loop).
# Each owns its own CMakeLists, so add the directory rather than a test here.
add_subdirectory(noop)
