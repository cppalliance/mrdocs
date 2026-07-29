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
            "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
            "--stdlib-includes=${LIBCXX_DIR}"
            "--libc-includes=${CMAKE_SOURCE_DIR}/data/mrdocs/headers/libc-stubs"
            --log-level=warn
    )
endforeach ()

# Script-driven generator example (a shell driver that invokes mrdocs).
add_test(
    NAME mrdocs-generator-script-driven-search-index
    COMMAND bash run.sh
            --addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons
            --output=${CMAKE_CURRENT_BINARY_DIR}/script-driven/search-index/reference-output
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/script-driven/search-index
)
set_property(TEST mrdocs-generator-script-driven-search-index PROPERTY
    ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")
