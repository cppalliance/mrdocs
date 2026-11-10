#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_add_checked_test(<name> OUTCOME <success|failure|build-failure> OUTPUT_REGEX <regex> [COMMAND <command>... | ARGS <argument>...])
#
# Register a ctest test that runs the `mrdocs` tool, or another command, and
# passes only if both its exit status and its output are the expected ones.
#
# Example
#
#   mrdocs_add_checked_test(mrdocs-plugin-extensions-is-a-file
#       OUTCOME success
#       OUTPUT_REGEX "The extensions directory \".*extensions-is-a-file/extensions\" could not be read"
#       ARGS
#           "--config=${CMAKE_CURRENT_SOURCE_DIR}/project/mrdocs.yml"
#           --generator=noop
#           "--addons-supplemental=${MRDOCS_FILE_ADDONS}")
#
# It registers the test `mrdocs-plugin-extensions-is-a-file`, which runs
#
#   cmake -DOUTCOME=success "-DREGEX=The extensions directory ..."
#         -P <this directory>/RunChecked.cmake
#         -- <path of mrdocs> --config=... --generator=noop --addons-supplemental=...
#
# The test fails if the exit status is not the one OUTCOME names, or if the
# output has no match of the regular expression.
#
# Parameters
#
#   <name>
#       The name of the test.
#   OUTCOME
#       `success` expects exit status zero. `failure` expects status 1, the one
#       `mrdocs` returns when it refuses to continue. `build-failure` expects
#       any status but zero, which is what a build tool returns when a compile
#       fails.
#   OUTPUT_REGEX
#       A regular expression that the combined standard output and standard
#       error has to match.
#   ARGS
#       The arguments of the `mrdocs` tool, one per line when there are many.
#   COMMAND
#       A command to run in place of the `mrdocs` tool, for a test that runs
#       the build tool, such as
#
#           mrdocs_add_checked_test(<name> OUTCOME build-failure
#               OUTPUT_REGEX <regex>
#               COMMAND ${CMAKE_COMMAND} --build <dir> --target <target>)
#
#   ARGS and COMMAND exclude each other. Whichever is given comes last, because
#   everything after it belongs to it.
#
# Preconditions
#
#   Without COMMAND, the `mrdocs` target exists. The function belongs to the
#   MrDocs build and is not part of the installed package.
#
# Guarantees
#
#   ctest's PASS_REGULAR_EXPRESSION looks at the output alone and ignores the
#   exit status, which is what tells a run that stopped from one that went on.
#   The test checks both, so a run that is refused and one that goes on cannot
#   be mistaken for each other.
# The -P script the test runs, which checks the exit status and the output.
# Tests that run a script of their own pass its path on to it, which is why the
# path is a variable and not read inside the function.
set(MRDOCS_RUN_CHECKED_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/RunChecked.cmake")

function(mrdocs_add_checked_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "OUTCOME;OUTPUT_REGEX" "ARGS;COMMAND")
    if (NOT arg_OUTCOME OR NOT DEFINED arg_OUTPUT_REGEX)
        message(FATAL_ERROR
            "mrdocs_add_checked_test(${name}): OUTCOME and OUTPUT_REGEX are required")
    endif ()
    if (arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "mrdocs_add_checked_test(${name}): unexpected arguments: "
            "${arg_UNPARSED_ARGUMENTS}")
    endif ()
    if (NOT arg_OUTCOME MATCHES "^(success|failure|build-failure)$")
        message(FATAL_ERROR
            "mrdocs_add_checked_test(${name}): OUTCOME must be success, failure "
            "or build-failure, not \"${arg_OUTCOME}\"")
    endif ()
    if (arg_COMMAND AND arg_ARGS)
        message(FATAL_ERROR
            "mrdocs_add_checked_test(${name}): COMMAND and ARGS exclude each other")
    endif ()
    if (arg_COMMAND)
        set(command ${arg_COMMAND})
    else ()
        set(command $<TARGET_FILE:mrdocs> ${arg_ARGS})
    endif ()
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            "-DOUTCOME=${arg_OUTCOME}"
            "-DREGEX=${arg_OUTPUT_REGEX}"
            -P "${MRDOCS_RUN_CHECKED_SCRIPT}"
            --
            ${command})
endfunction()
