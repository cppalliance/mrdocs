#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

if (MRDOCS_BUILD_HEADERS_ONLY OR NOT MRDOCS_BUILD_TESTS)
    return()
endif()

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
            --generator=noop
            "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
            "--stdlib-includes=${LIBCXX_DIR}"
            "--stdlib-includes=${STDLIB_INCLUDE_DIR}"
            "--libc-includes=${CMAKE_SOURCE_DIR}/data/mrdocs/headers/libc-stubs"
            --concurrency=16
            --log-level=debug
            $<$<BOOL:${MRDOCS_BUILD_STRICT_TESTS}>:--warn-as-error=true>
    )
