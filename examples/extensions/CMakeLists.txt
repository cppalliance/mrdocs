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
foreach (transform_example IN ITEMS ghostwriter project-conventions)
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

# Internal to MrDocs: keep warnings.txt (included by the Corpus Transforms page)
# in step with the tool. project-conventions-report re-captures the extension's
# warnings and fails if they drifted; mrdocs-project-conventions-report rewrites
# the file.
find_package(Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
    add_test(
        NAME project-conventions-report
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/project-conventions/report.py
                --check ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/project-conventions)
    set_tests_properties(project-conventions-report PROPERTIES
        ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")

    add_custom_target(mrdocs-project-conventions-report
        COMMAND ${CMAKE_COMMAND} -E env "MRDOCS=$<TARGET_FILE:mrdocs>"
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/project-conventions/report.py
                ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/project-conventions
        COMMENT "Regenerating project-conventions/warnings.txt"
        VERBATIM)
    add_dependencies(mrdocs-project-conventions-report mrdocs)
endif ()
