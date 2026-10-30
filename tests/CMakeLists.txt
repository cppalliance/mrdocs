#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Each test suite is a separate executable and owns its own CMakeLists:
#   unit/   - public-API unit tests (mrdocs-unit-tests)
#   golden/ - reference-output harness (mrdocs-golden-tests)
add_subdirectory(unit)
add_subdirectory(golden)
# Benchmarks are custom targets, not CTest tests; see benchmarks/README.md.
add_subdirectory(benchmarks)

# Self-documentation test: run mrdocs over its own build file with the noop
# generator (warn-as-error toggled by the strict flag).
set(MRDOCS_SELF_DOC_OUTPUT "${CMAKE_BINARY_DIR}/docs/self-reference")
add_test(NAME mrdocs-self-doc
    COMMAND
        mrdocs
        "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
            "--config=${CMAKE_SOURCE_DIR}/docs/mrdocs.yml"
            "--output=${MRDOCS_SELF_DOC_OUTPUT}"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            --generator=noop
            --log-level=debug
            $<$<BOOL:${MRDOCS_BUILD_STRICT_TESTS}>:--warn-as-error=true>
    )

# Diagnostics tests: how a malformed cond or endcond marker is reported. The
# script runs mrdocs on a tree under diagnostics/ and checks the output; see
# diagnostics/cond-markers/check.cmake for the meaning of each argument.
list(JOIN MRDOCS_BUILTIN_DIR_ARGS "|" MRDOCS_BUILTIN_DIR_ARGS_JOINED)
function (mrdocs_add_cond_markers_test name tree expect)
    cmake_parse_arguments(PARSE_ARGV 3 ARG "" "WARNINGS;FILES" "ARGS")
    list(JOIN ARG_ARGS "|" extra_args)
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DMRDOCS=$<TARGET_FILE:mrdocs>"
            "-DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/${tree}/mrdocs.yml"
            "-DOUTPUT_DIR=${CMAKE_BINARY_DIR}/tests/diagnostics/${name}"
            "-DEXTRA_ARGS=${MRDOCS_BUILTIN_DIR_ARGS_JOINED}|${extra_args}"
            "-DEXPECT=${expect}"
            "-DWARNINGS=${ARG_WARNINGS}"
            "-DFILES=${ARG_FILES}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/cond-markers/check.cmake"
        )
endfunction ()

set(MRDOCS_COND_MARKER_WARNINGS
    "include/unmatched.hpp:3:=unmatched|include/stray.hpp:7:=stray")
set(MRDOCS_COND_OUTSIDE_WARNINGS "outside/outside.hpp:7:=stray")

# Each marker is reported once, however many translation units include its file.
mrdocs_add_cond_markers_test(mrdocs-warn-cond-markers cond-markers once
    WARNINGS "${MRDOCS_COND_MARKER_WARNINGS}")

# The warnings are errors under warn-as-error, so the run fails.
mrdocs_add_cond_markers_test(mrdocs-warn-cond-markers-as-error cond-markers failure
    WARNINGS "${MRDOCS_COND_MARKER_WARNINGS}"
    ARGS --warn-as-error=true)

# No warning is printed when warnings are off.
mrdocs_add_cond_markers_test(mrdocs-warn-cond-markers-off cond-markers silent
    FILES "unmatched.hpp|stray.hpp"
    ARGS --warnings=false)

# A marker in a system header, or in a file that is not an input, is not
# reported as a warning, so warn-as-error does not fail.
mrdocs_add_cond_markers_test(mrdocs-warn-cond-markers-skipped cond-markers-skipped silent
    FILES "system.hpp|outside.hpp"
    ARGS --warn-as-error=true)

# A marker in a file that is not an input is still found, and reported at the
# debug level, and a marker in a system header is never reported, not even
# there.
mrdocs_add_cond_markers_test(mrdocs-warn-cond-markers-skipped-debug cond-markers-skipped seen
    WARNINGS "${MRDOCS_COND_OUTSIDE_WARNINGS}"
    FILES "system.hpp"
    ARGS --log-level=debug)
