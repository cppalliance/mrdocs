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
# compares the file it wrote with a committed one. On a mismatch it prints both,
# which `cmake -E compare_files` does not.
#
#   cmake -D EXPECTED=<file> -D ACTUAL=<file> -D RUN_COMMAND=<file>
#         -P CompareFiles.cmake -- <command> [<argument>...]
#
# RUN_COMMAND is the path of utils/cmake/helpers/RunCommand.cmake.

if (NOT RUN_COMMAND)
    message(FATAL_ERROR "CompareFiles.cmake: needs RUN_COMMAND")
endif ()
include("${RUN_COMMAND}")
message("${output}")
if (NOT status EQUAL 0)
    message(FATAL_ERROR "FAIL: expected exit status 0, got ${status}")
endif ()

foreach (file IN ITEMS EXPECTED ACTUAL)
    if (NOT EXISTS "${${file}}")
        message(FATAL_ERROR "FAIL: ${file} ${${file}} does not exist")
    endif ()
    file(READ "${${file}}" content_${file})
    # Line endings are ignored.
    string(REPLACE "\r" "" content_${file} "${content_${file}}")
endforeach ()

if (NOT content_EXPECTED STREQUAL content_ACTUAL)
    message(FATAL_ERROR
        "FAIL: ${ACTUAL} differs from ${EXPECTED}\n"
        "--- ${EXPECTED}\n${content_EXPECTED}\n"
        "--- ${ACTUAL}\n${content_ACTUAL}\n---")
endif ()
