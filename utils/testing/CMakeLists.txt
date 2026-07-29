#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Test-runner helpers, exposed as custom targets so they can be run at any
# time. These wrap the local/CI test flows; the individual suites are still
# registered as ordinary ctest tests by tests/ and the other utils/ dirs.
add_custom_target(run-all-tests
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_all_tests.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Configure, build, and run the full test suite (run_all_tests.py)")
add_custom_target(run-ci-with-act
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_ci_with_act.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Run the GitHub Actions workflows locally with act (run_ci_with_act.py)")
