#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Docs-tooling. The generator is a run-target (usable any time); the
# up-to-date check is a ctest test (CI "Utility Tests").
add_custom_target(generate-bootstrap-options
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_bootstrap_options.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Regenerate the bootstrap-options docs partial")

if (MRDOCS_BUILD_TESTS)
    add_test(NAME utils-bootstrap-options-check
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_bootstrap_options.py --check
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif ()
