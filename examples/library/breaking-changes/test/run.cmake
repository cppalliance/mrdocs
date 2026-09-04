#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

if(NOT TOOL OR NOT V1_CONFIG OR NOT V2_CONFIG OR NOT EXPECTED OR NOT ACTUAL)
    message(FATAL_ERROR "run.cmake: missing required variable")
endif()

# An installed consumer resolves MrDocs's resources from its root with no extra
# arguments. Running in-tree there is no <root>/share/mrdocs yet, so forward the
# built-in directory flags (BUILTIN_ARGS) as option overrides -- no copy, no
# machine paths baked into the example binary.
execute_process(
    COMMAND "${TOOL}" "${V1_CONFIG}" "${V2_CONFIG}" ${BUILTIN_ARGS}
    OUTPUT_FILE "${ACTUAL}"
    RESULT_VARIABLE _rc
)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Tool exited with status ${_rc}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${ACTUAL}" "${EXPECTED}"
    RESULT_VARIABLE _diff
)

if(NOT _diff EQUAL 0)
    message(FATAL_ERROR
        "Output differs from expected\n"
        "  actual:   ${ACTUAL}\n"
        "  expected: ${EXPECTED}\n"
        "Update the golden if the new output is intentional."
    )
endif()
