#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Formatting/metrics helpers, exposed as custom targets so they can be run at
# any time (they are not tied to tests).
add_custom_target(reformat
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/reformat.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Apply clang-format and include normalization (reformat.py)")
add_custom_target(reformat-check
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/reformat.py --check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Check formatting/include guards without modifying files")
add_custom_target(code-metrics
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/code_metrics.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Report code metrics (code_metrics.py)")
