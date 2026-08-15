#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Code generation owned by this directory (it holds the generator scripts).
# utils/ is added from the root before libs/, src/, tools/, and tests/, so the
# generated headers exist before every consumer that lists them as sources
# (mrdocs-core, the mrdocs tool, the golden tests) and before the src/ header
# build that parses them.

find_program(PYTHON_EXECUTABLE python3 python)
if (NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Python is needed to configure mrdocs")
endif()

# ---- include/mrdocs/Version.hpp from Version.hpp.in ----
set_ternary(MRDOCS_REQUIRE_GIT_OPT MRDOCS_REQUIRE_GIT REQUIRED QUIET)
find_package(Git ${MRDOCS_REQUIRE_GIT_OPT})
add_custom_target(mrdocs-version ALL
        BYPRODUCTS ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate-version-header.py
                ${PROJECT_SOURCE_DIR}/include/mrdocs/Version.hpp.in
                ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
                --version ${PROJECT_VERSION}
                --name ${PROJECT_NAME}
                --description "${PROJECT_DESCRIPTION}"
                --source-dir ${PROJECT_SOURCE_DIR}
                --git "${GIT_EXECUTABLE}"
        COMMENT "Generating Version.hpp"
        VERBATIM)

# ---- include/mrdocs/ConfigSchema.hpp from ConfigOptions.json  ----
set(CONFIG_GEN_WORKING_DIR      "${PROJECT_SOURCE_DIR}")
set(CONFIG_GEN_OUTPUT_DIR       "${PROJECT_BINARY_DIR}")
set(CONFIG_GEN_CONFIG_JSON      "${PROJECT_SOURCE_DIR}/src/mrdocs/ConfigOptions.json")
set(CONFIG_GEN_SCRIPT_INFO      "${CMAKE_CURRENT_SOURCE_DIR}/generate-config-info.py")
set(CONFIG_GEN_SCRIPT_SCHEMA    "${CMAKE_CURRENT_SOURCE_DIR}/generate-yaml-schema.py")
set(CONFIG_GEN_COMMAND_INFO ${PYTHON_EXECUTABLE} ${CONFIG_GEN_SCRIPT_INFO} ${CONFIG_GEN_CONFIG_JSON} ${CONFIG_GEN_OUTPUT_DIR})
set(CONFIG_GEN_COMMAND_SCHEMA ${PYTHON_EXECUTABLE} ${CONFIG_GEN_SCRIPT_SCHEMA})
set(CONFIG_GEN_INPUT_FILES
    ${CONFIG_GEN_SCRIPT_INFO}
    ${CONFIG_GEN_CONFIG_JSON}
    ${CONFIG_GEN_SCRIPT_SCHEMA}
)
set(CONFIG_GEN_OUTPUT_FILES
    ${CONFIG_GEN_OUTPUT_DIR}/include/mrdocs/ConfigSchema.hpp
)
# Generate at build time. The outputs are listed as sources by mrdocs-core, the
# mrdocs tool, and the golden tests; mrdocs-codegen gives those targets a stable
# dependency to order against across directories.
add_custom_command(
        OUTPUT ${CONFIG_GEN_OUTPUT_FILES}
        COMMAND ${CONFIG_GEN_COMMAND_INFO}
        COMMAND ${CONFIG_GEN_COMMAND_SCHEMA}
        WORKING_DIRECTORY ${CONFIG_GEN_WORKING_DIR}
        DEPENDS ${CONFIG_GEN_INPUT_FILES}
        COMMENT "Generating Config Source Files"
        VERBATIM
        COMMAND_EXPAND_LISTS
)
add_custom_target(mrdocs-codegen DEPENDS ${CONFIG_GEN_OUTPUT_FILES})
# mrdocs-core, the mrdocs tool, and the golden tests all depend on mrdocs-codegen;
# chaining the version target here orders it before them without extra wiring.
add_dependencies(mrdocs-codegen mrdocs-version)

# ---- schema check (mirrors the CI "Check YAML schema" step) ----
# The published config schema (docs/modules/ROOT/attachments/schemas/
# mrdocs.schema.json) must be up to date with the generator.
if (MRDOCS_BUILD_TESTS AND PYTHON_EXECUTABLE)
    add_test(NAME yaml-schema-check
             COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate-yaml-schema.py --check
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
