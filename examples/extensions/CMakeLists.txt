#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Corpus-transform extension examples exercised as run-only tests: the test
# only checks that mrdocs runs each example to completion using the example's
# own configuration. Output goes to the build tree; the committed output files
# are illustrative and are not compared here. The working directory is the
# example directory so a transform can resolve the external files it reads
# (for example ghostwriter's external-docs/) relative to the run.
foreach (transform_example IN ITEMS ghostwriter)
    add_test(
        NAME mrdocs-extension-example-${transform_example}
        COMMAND bash run.sh
                --output=${CMAKE_CURRENT_BINARY_DIR}/${transform_example}/reference-output
                ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${transform_example}
    )
    set_property(TEST mrdocs-extension-example-${transform_example} PROPERTY
        ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")
endforeach ()
