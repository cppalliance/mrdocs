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
# The input names a filtered symbol in each position the warning covers,
# and nothing else in it emits a warning: with the warning on, the run
# fails under --warn-as-error, and with it off the run is clean.
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

# The failure above is satisfied by any one warning, so each position is
# also checked on its own, as the declaration in the input that names a
# filtered symbol there and the symbol it names.
function(mrdocs_expect_filtered_in_public_api position referrer target)
    add_test(NAME mrdocs-warn-filtered-${position}
        COMMAND ${MRDOCS_FILTERED_API_COMMAND})
    set_tests_properties(mrdocs-warn-filtered-${position} PROPERTIES
        PASS_REGULAR_EXPRESSION "'${referrer}' names '${target}'")
endfunction()
mrdocs_expect_filtered_in_public_api(base-class T detail::my_shame)
mrdocs_expect_filtered_in_public_api(return-type publicFunction detail::my_shame)
mrdocs_expect_filtered_in_public_api(parameter takeShame detail::my_shame)
mrdocs_expect_filtered_in_public_api(alias shame_alias detail::my_shame)
mrdocs_expect_filtered_in_public_api(variable shameVariable detail::my_shame)
mrdocs_expect_filtered_in_public_api(data-member holder::member detail::my_shame)
mrdocs_expect_filtered_in_public_api(template-argument boxedShame detail::my_shame)
mrdocs_expect_filtered_in_public_api(specialization makeImpl detail::impl)

# The positions where a filtered symbol is named but must not be reported:
# a private base, a symbol marked `@implementationdefined`, a
# specialization of a documented template and a member reached through
# one.
add_test(NAME mrdocs-unreported-filtered-in-public-api
    COMMAND mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/filtered-not-reported/mrdocs.yml"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        --generator=noop
        --warn-as-error)

# The twin of mrdocs-ignore-filtered-outside-inputs: the same input, with
# the vendor directory among the inputs but excluded, is reported.
add_test(NAME mrdocs-warn-filtered-excluded-input
    COMMAND mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/filtered-outside-inputs/excluded-input.yml"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        --generator=noop
        --warn-as-error)
set_tests_properties(mrdocs-warn-filtered-excluded-input PROPERTIES
    PASS_REGULAR_EXPRESSION "'makeWidget' names 'vendor::widget'")
