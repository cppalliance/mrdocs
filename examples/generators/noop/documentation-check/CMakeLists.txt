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

if (NOT TARGET mrdocs)
    # tag::check[]
    find_package(mrdocs REQUIRED)
    add_test(
        NAME documentation-check
        COMMAND ${MRDOCS_EXECUTABLE}
                --config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
                --generator=noop --warn-as-error)
    # end::check[]
else ()
    # The just-built tool is not installed under its FHS layout yet, so point it
    # at the addons and bundled headers from the source tree.
    add_test(
        NAME documentation-check
        COMMAND $<TARGET_FILE:mrdocs>
                --config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
                --generator=noop --warn-as-error ${MRDOCS_BUILTIN_DIR_ARGS})
endif ()

# This example's header omits its @param docs on purpose, so the check is meant
# to catch it: its non-zero exit is the expected, passing result. A project with
# clean docs would not set this.
set_tests_properties(documentation-check PROPERTIES WILL_FAIL TRUE)

# Internal to MrDocs: keep report.txt (included by the No-op page) in step with
# the tool. documentation-check-report re-captures the report and fails if it
# drifted; the mrdocs-documentation-check-report target rewrites it.
if (TARGET mrdocs)
    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_FOUND)
        add_test(
            NAME documentation-check-report
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/report.py
                    --check ${MRDOCS_BUILTIN_DIR_ARGS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(documentation-check-report PROPERTIES
            ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")

        add_custom_target(mrdocs-documentation-check-report
            COMMAND ${CMAKE_COMMAND} -E env "MRDOCS=$<TARGET_FILE:mrdocs>"
                    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/report.py
                    ${MRDOCS_BUILTIN_DIR_ARGS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Regenerating documentation-check/report.txt"
            VERBATIM)
        add_dependencies(mrdocs-documentation-check-report mrdocs)
    endif ()
endif ()
