#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Metrics helper, exposed as a custom target so it can be run at any time
# (it is not tied to tests).
add_custom_target(code-metrics
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/code_metrics.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Report code metrics (code_metrics.py)")
