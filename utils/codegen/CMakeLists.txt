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

# ---- include/mrdocs/Version.hpp from Version.hpp.in ----
if (MRDOCS_REQUIRE_GIT)
    find_package(Git REQUIRED)
else()
    find_package(Git QUIET)
endif()
set(PROJECT_VERSION_BUILD "")
set(PROJECT_VERSION_WITH_BUILD "${PROJECT_VERSION}")  # default: plain semver
if (EXISTS "${PROJECT_SOURCE_DIR}/.git" AND GIT_FOUND)
    # Get full SHA
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_SHA_FULL
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_SHA_FULL_RV
            ERROR_QUIET
    )
    if (NOT GIT_SHA_FULL_RV EQUAL 0)
        message(FATAL_ERROR "Git was found but could not extract commit SHA")
    endif()
    set(PROJECT_VERSION_BUILD "${GIT_SHA_FULL}")
    string(SUBSTRING "${GIT_SHA_FULL}" 0 12 GIT_SHA_SHORT)

    # Are we exactly at a tag?
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_EXACT_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_EXACT_TAG_RV
            ERROR_QUIET
    )
    if (GIT_EXACT_TAG_RV EQUAL 0)
        # On a tag: canonical release
        set(PROJECT_VERSION_WITH_BUILD "${PROJECT_VERSION}")
    else()
        # Dirty working tree? (0 = clean, 1 = dirty)
        execute_process(
                COMMAND ${GIT_EXECUTABLE} diff --quiet --ignore-submodules
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                RESULT_VARIABLE GIT_DIRTY_RV
                ERROR_QUIET
        )
        set(_dirty_suffix "")
        if (GIT_DIRTY_RV EQUAL 1)
            set(_dirty_suffix ".modified")
        endif()
        set(PROJECT_VERSION_WITH_BUILD
                "${PROJECT_VERSION}+${GIT_SHA_SHORT}${_dirty_suffix}")
    endif()
else()
    if (MRDOCS_REQUIRE_GIT)
        message(FATAL_ERROR "Git is required to extract the version build")
    endif()
    set(PROJECT_VERSION_BUILD "")
    set(PROJECT_VERSION_WITH_BUILD "${PROJECT_VERSION}")
endif()
configure_file(
        ${PROJECT_SOURCE_DIR}/include/mrdocs/Version.hpp.in
        ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
        @ONLY
)

# ---- include/mrdocs/ConfigSchema.hpp + related files from the JSON config ----
find_program(PYTHON_EXECUTABLE python3 python)
if (NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Python is needed to configure mrdocs")
endif()
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
    ${CONFIG_GEN_OUTPUT_DIR}/src/lib/Lib/ConfigSchema.cpp
    ${CONFIG_GEN_OUTPUT_DIR}/src/tool/PublicToolArgs.hpp
    ${CONFIG_GEN_OUTPUT_DIR}/src/tool/PublicToolArgs.cpp
)
if (MRDOCS_BUILD_HEADERS_ONLY)
    # Create the files at configure time: the src/ header build parses the
    # generated public headers immediately and defines no build-time targets.
    execute_process(
            COMMAND ${CONFIG_GEN_COMMAND_INFO}
            WORKING_DIRECTORY ${CONFIG_GEN_WORKING_DIR}
            COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
            COMMAND ${CONFIG_GEN_COMMAND_SCHEMA}
            WORKING_DIRECTORY ${CONFIG_GEN_WORKING_DIR}
            COMMAND_ERROR_IS_FATAL ANY
    )
    set_source_files_properties(${CONFIG_GEN_OUTPUT_FILES} PROPERTIES GENERATED TRUE)
else()
    # Generate at build time. The outputs are listed as sources by mrdocs-core,
    # the mrdocs tool, and the golden tests; mrdocs-codegen gives those targets
    # a stable dependency to order against across directories.
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
endif()

# ---- schema check (mirrors the CI "Check YAML schema" step) ----
# The published config schema (docs/modules/ROOT/attachments/schemas/
# mrdocs.schema.json) must be up to date with the generator.
if (MRDOCS_BUILD_TESTS AND PYTHON_EXECUTABLE)
    add_test(NAME yaml-schema-check
             COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate-yaml-schema.py --check
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
