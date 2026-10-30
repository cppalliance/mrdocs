#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Run MrDocs on a tree whose headers hold a malformed cond or endcond marker,
# and check how the markers are reported. The source files are found from the
# source-root of the configuration file.
#
# Inputs, passed with -D:
#   MRDOCS         the mrdocs executable
#   CONFIG         the configuration file of the tree
#   OUTPUT_DIR     where the (unused) generator output goes
#   EXTRA_ARGS     further mrdocs arguments, separated by "|"
#   EXPECT         what the run must show, one of:
#                  once      success, each warning of WARNINGS exactly once
#                  failure   failure, each warning of WARNINGS present
#                  seen      success, each warning of WARNINGS present, and
#                            none of the files named in FILES mentioned
#                  silent    success, no marker warning, and none of the files
#                            named in FILES mentioned
#   WARNINGS       the expected warnings, as "file:line=kind" entries
#                  separated by "|", where the kind is "unmatched" for a cond
#                  with no endcond and "stray" for an endcond with no cond
#   FILES          file names, separated by "|"

# Check that `output` holds exactly one `location` (a file:line) and that the
# warning at it reads as `wording`. The occurrences are counted on the
# location alone, since the wording holds a ";" that would split the list of
# matches.
function (expect_once location wording)
    string(REGEX MATCHALL "${location}" found "${output}")
    list(LENGTH found count)
    if (NOT count EQUAL 1)
        message(FATAL_ERROR
            "expected exactly one '${location}', found ${count}:\n${output}")
    endif ()
    expect_present("${location}" "${wording}")
endfunction ()

# Check that `output` holds a warning that reads as `wording` at `location`.
function (expect_present location wording)
    if (NOT output MATCHES "${location} [^\n]*${wording}")
        message(FATAL_ERROR
            "expected '${wording}' at '${location}':\n${output}")
    endif ()
endfunction ()

# Check that `output` does not mention the files named in FILES.
function (expect_not_mentioned)
    string(REPLACE "|" ";" files "${FILES}")
    foreach (file IN LISTS files)
        if (output MATCHES "${file}")
            message(FATAL_ERROR "unexpected mention of '${file}':\n${output}")
        endif ()
    endforeach ()
endfunction ()

# Check that `output` holds none of the marker warnings and does not mention
# the files named in FILES.
function (expect_silent)
    if (output MATCHES "has no matching")
        message(FATAL_ERROR "unexpected marker warning:\n${output}")
    endif ()
    expect_not_mentioned()
endfunction ()

# Check each "location=kind" entry of WARNINGS against `output`, with the
# regular expression the warning of that kind matches. Each warning must be
# present, and exactly once if `once` is true.
function (check_warnings once)
    set(wording_unmatched "has no matching [^\n]*the end of the file")
    set(wording_stray "has no matching [^\n]*it is ignored")
    string(REPLACE "|" ";" entries "${WARNINGS}")
    foreach (entry IN LISTS entries)
        string(REGEX REPLACE "=.*" "" location "${entry}")
        string(REGEX REPLACE "^[^=]*=" "" kind "${entry}")
        set(wording "${wording_${kind}}")
        if (once)
            expect_once("${location}" "${wording}")
        else ()
            expect_present("${location}" "${wording}")
        endif ()
    endforeach ()
endfunction ()

string(REPLACE "|" ";" extra_args "${EXTRA_ARGS}")
execute_process(
    COMMAND "${MRDOCS}"
        "--config=${CONFIG}"
        "--output=${OUTPUT_DIR}"
        --generator=noop
        ${extra_args}
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors)
set(output "${output}${errors}")

if (EXPECT STREQUAL "failure")
    if (exit_code EQUAL 0)
        message(FATAL_ERROR "mrdocs succeeded, it should have failed:\n${output}")
    endif ()
elseif (NOT exit_code EQUAL 0)
    message(FATAL_ERROR "mrdocs failed with ${exit_code}:\n${output}")
endif ()

if (EXPECT STREQUAL "once")
    check_warnings(TRUE)
elseif (EXPECT STREQUAL "failure")
    check_warnings(FALSE)
elseif (EXPECT STREQUAL "seen")
    check_warnings(FALSE)
    expect_not_mentioned()
elseif (EXPECT STREQUAL "silent")
    expect_silent()
else ()
    message(FATAL_ERROR "unknown EXPECT '${EXPECT}'")
endif ()
