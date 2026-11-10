#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Runs the command after `--`, for the `-P` scripts that check what a command
# does. A script includes it by path: RunChecked.cmake from this directory, and
# the refusal-message comparison under tests/ and the comparison scripts of
# examples/extensions/ with the path they are given in -DRUN_COMMAND.
#
#   include("${CMAKE_CURRENT_LIST_DIR}/RunCommand.cmake")
#   include("${RUN_COMMAND}")
#
# It sets `command` to the arguments after `--`, `status` to the exit status
# and `output` to the combined standard output and standard error.

set(command)
set(past_separator FALSE)
math(EXPR last "${CMAKE_ARGC} - 1")
foreach (index RANGE 0 ${last})
    if (past_separator)
        list(APPEND command "${CMAKE_ARGV${index}}")
    elseif (CMAKE_ARGV${index} STREQUAL "--")
        set(past_separator TRUE)
    endif ()
endforeach ()
# It stops with an error when no command follows `--`.
if (NOT command)
    get_filename_component(script "${CMAKE_SCRIPT_MODE_FILE}" NAME)
    message(FATAL_ERROR "${script}: no command after --")
endif ()

execute_process(
    COMMAND ${command}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
