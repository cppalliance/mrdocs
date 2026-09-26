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

# A warning is not a generated file, so the golden harness cannot see it.
# The input names a filtered symbol twice, as a base class and as a return
# type, and nothing else in it emits a warning: with the warning on, the
# run fails under --warn-as-error, and with it off the run is clean.
set(MRDOCS_FILTERED_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/filtered-in-public-api")
set(MRDOCS_FILTERED_API_COMMAND
        mrdocs
            "--config=${MRDOCS_FILTERED_API_DIR}/mrdocs.yml"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            --generator=noop
            --warn-as-error)
add_test(NAME mrdocs-warn-filtered-in-public-api
    COMMAND ${MRDOCS_FILTERED_API_COMMAND})
set_tests_properties(mrdocs-warn-filtered-in-public-api PROPERTIES WILL_FAIL TRUE)

add_test(NAME mrdocs-quiet-filtered-in-public-api
    COMMAND ${MRDOCS_FILTERED_API_COMMAND} --warn-if-filtered-in-public-api=false)

add_test(NAME mrdocs-ignore-filtered-outside-inputs
    COMMAND mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/filtered-outside-inputs/mrdocs.yml"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        --generator=noop
        --warn-as-error)
