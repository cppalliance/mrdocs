#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Runs the command after `--`, which has to exit with status 1, the status of
# a refusal, and compares the message it reports with a committed file. The
# documentation includes the file, so the messages it shows are the ones the
# tool prints.
#
#   cmake -D EXPECTED=<file> -D RUN_COMMAND=<file> [-D UPDATE=ON] -P CompareMessage.cmake -- <command> [<argument>...]
#
# RUN_COMMAND is the path of utils/cmake/helpers/RunCommand.cmake.
#
# The message is the text after "Generating reference failed: ". The parts that
# depend on the machine are replaced before the comparison. With UPDATE the file
# is rewritten with the message instead of being compared.

if (NOT EXPECTED)
    message(FATAL_ERROR "CompareMessage.cmake: needs EXPECTED")
endif ()
if (NOT RUN_COMMAND)
    message(FATAL_ERROR "CompareMessage.cmake: needs RUN_COMMAND")
endif ()
include("${RUN_COMMAND}")
if (NOT status STREQUAL "1")
    message(FATAL_ERROR "FAIL: expected exit status 1, got ${status}\n${output}")
endif ()

string(ASCII 27 esc)
string(REGEX REPLACE "${esc}\\[[0-9;]*m" "" output "${output}")
if (NOT output MATCHES "Generating reference failed: ([^\n]*)")
    message(FATAL_ERROR "FAIL: no refusal in the output:\n${output}")
endif ()
set(message "${CMAKE_MATCH_1}")
# The path of the plugin is replaced by its file name with the platform's prefix
# and extension removed.
if (message MATCHES "^the plugin \"([^\"]*)\"(.*)$")
    set(path "${CMAKE_MATCH_1}")
    set(rest "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "^.*[/\\\\]" "" file "${path}")
    string(REGEX REPLACE "^lib" "" file "${file}")
    string(REGEX REPLACE "\\.(dylib|dll)$" ".so" file "${file}")
    set(message "the plugin \"${file}\"${rest}")
endif ()
# The build id and the standard library tag of this MrDocs, and the reason the
# system loader gives, are replaced by placeholders.
string(REGEX REPLACE "and this is build [^;]*;" "and this is build <build id>;" message "${message}")
string(REGEX REPLACE "and this MrDocs with [^;]*;" "and this MrDocs with <standard library>;" message "${message}")
string(REGEX REPLACE "could not be loaded: .*$" "could not be loaded: <reason from the system loader>" message "${message}")
# The path of a Lua script is cut to its file name, a chunk name such as
# `[string "..."]` becomes `[script]`, and the source location the tool appends
# is removed.
string(REGEX REPLACE "[^ \"]*[/\\\\]([A-Za-z0-9_.-]+\\.lua)" "\\1" message "${message}")
string(REGEX REPLACE "\\[string \"[^\"]*\"\\]" "[script]" message "${message}")
string(REGEX REPLACE " \\(src[/\\\\][^)]*\\)$" "" message "${message}")
string(STRIP "${message}" message)

# With UPDATE the file is rewritten.
if (UPDATE)
    file(WRITE "${EXPECTED}" "${message}\n")
else ()
    file(READ "${EXPECTED}" expected)
    string(STRIP "${expected}" expected)
    if (NOT message STREQUAL expected)
        message(FATAL_ERROR "FAIL: the message differs\n--- expected\n${expected}\n--- got\n${message}")
    endif ()
endif ()
