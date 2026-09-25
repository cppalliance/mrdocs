#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

cmake_minimum_required(VERSION 3.20)

# Built on its own, this example is a small standalone project that finds an
# installed MrDocs. Built as part of MrDocs itself (the parent adds this
# directory), the project, CTest, and the mrdocs target already exist.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(documentation-check LANGUAGES NONE)
    enable_testing()
endif ()

# This example's header omits its @param docs on purpose, so the check is meant
# to catch it: its non-zero exit is the expected, passing result. A project with
# clean docs would not set WILL_FAIL.
if (NOT TARGET mrdocs)
    # tag::check[]
    find_package(mrdocs REQUIRED)
    add_test(
        NAME documentation-check
        COMMAND ${MRDOCS_EXECUTABLE}
                --config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
                --generator=noop --warn-as-error)
    # end::check[]
    set_tests_properties(documentation-check PROPERTIES WILL_FAIL TRUE)
else ()
    # Built as part of MrDocs, the same check runs as one of its examples.
    mrdocs_add_example(DIRECTORY . CONFIG mrdocs.yml
        ARGS --generator=noop --warn-as-error RUN_ONLY WILL_FAIL)

    # report.txt, which the No-op page includes, is the check's output as
    # report.py captures it; it is compared with a fresh capture.
    mrdocs_add_example(DIRECTORY . NAME generators-noop-documentation-check-report
        SCRIPT report.py OUTPUTS report.txt)
endif ()
