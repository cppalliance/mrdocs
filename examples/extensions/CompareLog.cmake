#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Runs the command after `--`, which has to exit with status zero, and
# compares the lines it logged with a committed file.
#
#   cmake -D FILTER=<regular expression> -D EXPECTED=<file>
#         -D RUN_COMMAND=<file> [-D SORT=ON]
#         -P CompareLog.cmake -- <command> [<argument>...]
#
# RUN_COMMAND is the path of utils/cmake/helpers/RunCommand.cmake.
# EXPECTED is the committed file the lines are compared with. The lines of the
# combined standard output and standard error that match FILTER, whole and with
# their indentation, are what is compared. With SORT the lines are sorted and
# repeated ones are dropped first.

if (NOT RUN_COMMAND)
    message(FATAL_ERROR "CompareLog.cmake: needs RUN_COMMAND")
endif ()
include("${RUN_COMMAND}")
message("${output}")
if (NOT status EQUAL 0)
    message(FATAL_ERROR "FAIL: expected exit status 0, got ${status}")
endif ()

# The color codes are removed from the output.
string(ASCII 27 esc)
string(REGEX REPLACE "${esc}\\[[0-9;]*m" "" output "${output}")
# Line endings are ignored.
string(REPLACE "\r" "" output "${output}")

# The output is split into lines, and the ones that match FILTER are kept.
set(lines)
set(rest "${output}")
while (rest MATCHES "^([^\n]*)\n(.*)$")
    set(line "${CMAKE_MATCH_1}")
    set(rest "${CMAKE_MATCH_2}")
    if (line MATCHES "${FILTER}")
        list(APPEND lines "${line}")
    endif ()
endwhile ()
# With SORT the lines are sorted and repeated ones are dropped, for a log whose
# order is not fixed.
if (SORT)
    list(SORT lines)
    list(REMOVE_DUPLICATES lines)
endif ()
set(actual "")
foreach (line IN LISTS lines)
    string(APPEND actual "${line}\n")
endforeach ()

if (NOT EXPECTED)
    message(FATAL_ERROR "CompareLog.cmake: EXPECTED is not set")
endif ()
file(READ "${EXPECTED}" expected)
string(REPLACE "\r" "" expected "${expected}")
if (NOT actual STREQUAL expected)
    message(FATAL_ERROR
        "FAIL: the logged lines differ from ${EXPECTED}\n"
        "--- expected\n${expected}--- actual\n${actual}---")
endif ()
