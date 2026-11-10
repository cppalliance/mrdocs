#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# run.sh runs the example with its own mrdocs.yml, which has to complete. The
# cpp run logs the warnings that warnings.txt holds, as the page includes it.
mrdocs_add_extension_example(project-conventions MODE report RUN_SH
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}"
    FILTER ": low-quality brief: " SORT
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/warnings.txt")

# Internal to MrDocs: keep warnings.txt in step with the tool.
find_package(Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
    # Re-captures the extension's warnings and fails if they drifted.
    add_test(
        NAME project-conventions-report
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/report.py
                --check ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(project-conventions-report PROPERTIES
        ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")

    # Rewrites the file.
    add_custom_target(mrdocs-project-conventions-report
        COMMAND ${CMAKE_COMMAND} -E env "MRDOCS=$<TARGET_FILE:mrdocs>"
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/report.py
                ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerating project-conventions/warnings.txt"
        VERBATIM)
    add_dependencies(mrdocs-project-conventions-report mrdocs)
endif ()
