#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Runs one example and checks or updates its committed output. Invoked by
# the test and the update target that mrdocs_add_example creates (see
# MrDocsExample.cmake), never by hand:
#
#   cmake -DMODE=check|update -DNAME=<example> -DEXAMPLE_DIR=<dir>
#         -DOUTPUT_DIR=<dir> -DMRDOCS=<exe> -DCOMMAND=<command>
#         [-DOUTPUTS=<committed>[=<generated>];...]
#         [-DUPDATE_TARGET=<target>] -P run-example.cmake
#
# COMMAND runs in EXAMPLE_DIR, since several examples read files relative
# to the run, and writes into OUTPUT_DIR, which is emptied first. Each
# committed file named in OUTPUTS is compared with the file the run wrote
# under the same path, or under the path after `=` where the two differ.
# Line endings are ignored, so a CRLF checkout compares equal.

foreach (var MODE NAME EXAMPLE_DIR OUTPUT_DIR MRDOCS COMMAND)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "run-example.cmake: ${var} is not set")
    endif ()
endforeach ()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# A script that runs mrdocs itself finds it here.
set(ENV{MRDOCS} "${MRDOCS}")
execute_process(
    COMMAND ${COMMAND}
    WORKING_DIRECTORY "${EXAMPLE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "${NAME}: the example failed (${result}):\n${output}")
endif ()

set(drifted "")
foreach (entry IN LISTS OUTPUTS)
    string(FIND "${entry}" "=" split)
    if (split EQUAL -1)
        set(committed "${entry}")
        set(generated "${entry}")
    else ()
        string(SUBSTRING "${entry}" 0 ${split} committed)
        math(EXPR split "${split} + 1")
        string(SUBSTRING "${entry}" ${split} -1 generated)
    endif ()
    set(from "${OUTPUT_DIR}/${generated}")
    set(to "${EXAMPLE_DIR}/${committed}")
    if (NOT EXISTS "${from}")
        message(FATAL_ERROR "${NAME}: the example did not write ${generated}")
    endif ()
    if (MODE STREQUAL "update")
        file(COPY_FILE "${from}" "${to}")
    else ()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol
                    "${from}" "${to}"
            RESULT_VARIABLE differs)
        if (NOT differs EQUAL 0)
            list(APPEND drifted "${committed}")
        endif ()
    endif ()
endforeach ()

if (drifted)
    list(JOIN drifted "\n  " drifted)
    message(FATAL_ERROR
        "${NAME}: the committed output no longer matches the example:\n"
        "  ${drifted}\n"
        "If the new output is intended, build the ${UPDATE_TARGET} target "
        "to update it.")
endif ()
