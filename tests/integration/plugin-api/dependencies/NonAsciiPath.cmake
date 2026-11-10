#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Copy the addon root FROM to one whose name is not ASCII. Then run the tool
# through utils/cmake/helpers/RunChecked.cmake with a configuration that names
# the copy, and expect a run that succeeds and prints a line matching REGEX.
# Arguments, as -D
# definitions: FROM, the addon root to copy. WORK, a directory this script
# empties and owns. SOURCE_ROOT, the source root the configuration names, which
# holds sample.cpp. MRDOCS, the executable. BUILTIN_ARGS, the in-tree directory
# flags with | between them. RUN_CHECKED, the path of RunChecked.cmake. REGEX,
# the pattern the output has to match.

if (NOT FROM OR NOT WORK OR NOT SOURCE_ROOT OR NOT MRDOCS
        OR NOT RUN_CHECKED OR NOT REGEX)
    message(FATAL_ERROR "NonAsciiPath.cmake: missing required variable")
endif ()

# An e with an acute accent, as its UTF-8 bytes, so that this file stays
# ASCII.
string(ASCII 195 169 e_acute)
set(addons "${WORK}/addons-${e_acute}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${addons}")
# This script makes the copy rather than the build, which may run its commands
# through the ANSI code page as well.
file(COPY "${FROM}/extensions" DESTINATION "${addons}")

# The configuration file names the copy, because MrDocs reads that file as UTF-8
# and on Windows the command line arrives in the ANSI code page.
set(config "${WORK}/mrdocs.yml")
file(WRITE "${config}"
    "source-root: \"${SOURCE_ROOT}\"\n"
    "file-patterns:\n"
    "  - sample.cpp\n"
    "warn-if-undocumented: false\n"
    "addons-supplemental:\n"
    "  - \"${addons}\"\n")

# A ; would split the definition that carries the flags, so they come with |
# between them.
string(REPLACE "|" ";" builtin_args "${BUILTIN_ARGS}")
execute_process(
    COMMAND ${CMAKE_COMMAND}
        "-DOUTCOME=success"
        "-DREGEX=${REGEX}"
        -P "${RUN_CHECKED}"
        --
        "${MRDOCS}"
        "--config=${config}"
        --generator=noop
        "--output=${WORK}/output"
        ${builtin_args}
        --log-level=info
    RESULT_VARIABLE status)
if (NOT status EQUAL 0)
    message(FATAL_ERROR "The run from the non-ASCII addon root failed")
endif ()
