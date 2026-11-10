#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#


# Formatter, exposed as custom targets so it can be run at any time (they are
# not tied to tests).
add_custom_target(format
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/format.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Apply clang-format and include normalization to the changed lines (format.py)")
add_custom_target(format-check
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/format.py --check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Report the lines format.py would change, without modifying files")
