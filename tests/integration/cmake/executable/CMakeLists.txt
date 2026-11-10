#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Runs the mrdocs tool on a project, as a user of the executable does.

# A plain library, present so there is some project code to document.
add_library(example_calc src/calculator.cpp)
target_include_directories(example_calc PUBLIC include)
target_compile_features(example_calc PUBLIC cxx_std_20)
add_custom_target(consumer-docs ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/reference
        COMMAND ${MRDOCS_EXECUTABLE}
            --config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
            --compilation-database=${CMAKE_CURRENT_SOURCE_DIR}/docs/compile_commands.json
            --output=${CMAKE_CURRENT_BINARY_DIR}/reference
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/example/calculator.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
                ${CMAKE_CURRENT_SOURCE_DIR}/docs/compile_commands.json
        COMMENT "Generate consumer docs with MrDocs"
        USES_TERMINAL)
