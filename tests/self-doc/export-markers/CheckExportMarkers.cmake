#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Runs the export-markers check over the cases in this directory.
#
# `cases/cases.hpp` holds the classes and functions the check must accept and
# the ones it must flag, and `expected.txt` lists the errors it reports, one
# per line. `rejected/rejected.hpp` holds MRDOCS_VISIBLE on a member
# and on a function, which does not parse: the run must fail with one clang
# error for each.
#
# Inputs: MRDOCS, MRDOCS_ARGS (the arguments joined with `|`), CASES_DIR and
# OUTPUT_DIR.

cmake_minimum_required(VERSION 3.13)

string(REPLACE "|" ";" mrdocs_args "${MRDOCS_ARGS}")

# Runs mrdocs over one header of the cases and returns its exit code and its
# whole output.
function(run_mrdocs name result_var output_var)
    execute_process(
        COMMAND "${MRDOCS}" "${CASES_DIR}/${name}/${name}.hpp"
            "--source-root=${CASES_DIR}/${name}"
            "--output=${OUTPUT_DIR}/${name}"
            ${mrdocs_args}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    set(${result_var} "${result}" PARENT_SCOPE)
    set(${output_var} "${output}" PARENT_SCOPE)
endfunction()

# The error lines of an output, sorted. A message holds a `;`, which a CMake
# list would split, so it is swapped for a placeholder while the lines are
# listed, and the color codes that can precede a line are removed.
function(error_lines text result_var)
    string(ASCII 27 escape)
    string(REGEX REPLACE "${escape}\\[[0-9;]*m" "" text "${text}")
    string(REPLACE ";" "<semicolon>" text "${text}")
    string(REGEX MATCHALL "(^|\n)mrdocs::[^\n]*" matches "${text}")
    set(lines "")
    foreach (line IN LISTS matches)
        string(STRIP "${line}" line)
        list(APPEND lines "${line}")
    endforeach ()
    list(SORT lines)
    set(${result_var} "${lines}" PARENT_SCOPE)
endfunction()

run_mrdocs(cases cases_result cases_output)
error_lines("${cases_output}" actual)
file(READ "${CASES_DIR}/expected.txt" expected_text)
error_lines("${expected_text}" expected)
if (NOT actual STREQUAL expected)
    list(JOIN expected "\n  " expected_list)
    list(JOIN actual "\n  " actual_list)
    message(FATAL_ERROR
        "The export-markers check did not report the errors in expected.txt.\n"
        "Expected:\n  ${expected_list}\n"
        "Reported:\n  ${actual_list}\n"
        "Output:\n${cases_output}")
endif ()
string(REGEX MATCH "[^\n]*(error:|stack traceback)[^\n]*" failure "${cases_output}")
if (failure)
    message(FATAL_ERROR
        "The cases run hit a clang error or a Lua traceback: ${failure}\n"
        "Output:\n${cases_output}")
endif ()
if (cases_result EQUAL 0)
    message(FATAL_ERROR "The export-markers check reported errors and mrdocs exited with 0")
endif ()

run_mrdocs(rejected rejected_result rejected_output)
string(REGEX MATCHALL "error:[^\n]*type_visibility"
    parse_errors "${rejected_output}")
list(LENGTH parse_errors parse_error_count)
if (rejected_result EQUAL 0 OR NOT parse_error_count EQUAL 3)
    message(FATAL_ERROR
        "MRDOCS_VISIBLE on a member and on a function must stop the parse "
        "with three clang errors about type_visibility (exit ${rejected_result}, ${parse_error_count} errors).\n"
        "Output:\n${rejected_output}")
endif ()
