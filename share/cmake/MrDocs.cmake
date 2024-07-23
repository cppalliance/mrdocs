#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

#[=======================================================================[.rst:
MrDocs
-----

This module provides a function to generate documentation from C++ source
files with mrdocs.

It identifies the necessary MrDocs configuration options for the current
project and generates a documentation target.

See the MrDocs usage documentation for complete instructions.

#]=======================================================================]

function(add_mrdocs MRDOCS_TARGET_NAME)
    #-------------------------------------------------
    # Parse arguments
    #-------------------------------------------------
    set(options_prefix MRDOCS_TARGET)
    set(options EXCLUDE_FROM_ALL)
    set(oneValueArgs CONFIG OUTPUT ADDONS)
    set(multiValueArgs COMMENT)
    cmake_parse_arguments(${options_prefix} "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(MRDOCS_TARGET_SOURCES ${MRDOCS_TARGET_UNPARSED_ARGUMENTS})

    #-------------------------------------------------
    # Executable
    #-------------------------------------------------
    if (NOT DEFINED MRDOCS_EXECUTABLE AND TARGET mrdocs)
        set(MRDOCS_EXECUTABLE $<TARGET_FILE:mrdocs>)
        set(MRDOCS_EXECUTABLE_DEPENDENCY mrdocs)
    endif()
    if (NOT DEFINED MRDOCS_EXECUTABLE)
        find_program(MRDOCS_EXECUTABLE mrdocs)
        if (NOT MRDOCS_EXECUTABLE)
            message(FATAL_ERROR "MrDocs build script requires mrdocs to be installed")
        endif()
    endif()

    #-------------------------------------------------
    # CMake compile commands
    #-------------------------------------------------
    if (NOT DEFINED MRDOCS_COMPILE_COMMANDS)
        if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
            message(FATAL_ERROR "MrDocs requires either CMAKE_EXPORT_COMPILE_COMMANDS=ON or MRDOCS_COMPILE_COMMANDS to be set")
        endif()
        set(MRDOCS_COMPILE_COMMANDS ${CMAKE_BINARY_DIR}/compile_commands.json)
    endif()
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

    #-------------------------------------------------
    # EXCLUDE_FROM_ALL
    #-------------------------------------------------
    if (MRDOCS_TARGET_EXCLUDE_FROM_ALL)
        set(MRDOCS_TARGET_ALL_STR "")
    else()
        set(MRDOCS_TARGET_ALL_STR "ALL")
    endif()

    #-------------------------------------------------
    # Configuration
    #-------------------------------------------------
    if (NOT MRDOCS_TARGET_CONFIG)
        set(MRDOCS_TARGET_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml)
        if (NOT EXISTS ${MRDOCS_TARGET_CONFIG})
            foreach (dir doc docs antora)
                if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdocs.yml)
                    set(MRDOCS_TARGET_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdocs.yml)
                    break()
                endif()
            endforeach()
        endif()
        if (NOT EXISTS ${MRDOCS_TARGET_CONFIG})
            foreach (dir ${CMAKE_CURRENT_SOURCE_DIR})
                if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdocs.yml)
                    set(MRDOCS_TARGET_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdocs.yml)
                    break()
                endif()
            endforeach()
        endif()
        if (NOT EXISTS ${MRDOCS_TARGET_CONFIG})
            message(FATAL_ERROR "MrDocs: CONFIG option not set and no mrdocs.yml found in ${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
    endif()
    get_filename_component(MRDOCS_TARGET_CONFIG ${MRDOCS_TARGET_CONFIG} ABSOLUTE)

    #-------------------------------------------------
    # Format
    #-------------------------------------------------
    if (MRDOCS_TARGET_ADDONS)
        if (NOT EXISTS ${MRDOCS_TARGET_ADDONS})
            message(FATAL_ERROR "MrDocs: ADDONS directory ${MRDOCS_TARGET_ADDONS} does not exist")
        endif()
    endif()

    if (NOT MRDOCS_TARGET_ADDONS)
        get_filename_component(MRDOCS_EXECUTABLE_DIR ${MRDOCS_EXECUTABLE} DIRECTORY)
        set(${PROJECT_NAME}_DIR ${CMAKE_CURRENT_SOURCE_DIR})
        set(DEFAULT_ADDONS_PATHS
            # Project has its own addons
            ${${PROJECT_NAME}_DIR}/share/mrdocs/addons
            ${${PROJECT_NAME}_DIR}/docs/mrdocs/addons
            ${${PROJECT_NAME}_DIR}/docs/addons
            ${${PROJECT_NAME}_DIR}/antora/addons
            # Relative to mrdocs executable
            ${MRDOCS_EXECUTABLE_DIR}/../share/mrdocs/addons # FHS
            ${MRDOCS_EXECUTABLE_DIR}/share/mrdocs/addons    # FHS with no `bin`
            ${MRDOCS_EXECUTABLE_DIR}/../addons             # Non-FHS
            ${MRDOCS_EXECUTABLE_DIR}/addons                # Non-FHS with no `bin`
        )
        foreach (dir ${DEFAULT_ADDONS_PATHS})
            message(STATUS "MrDocs: Looking for addons in ${dir}")
            if (EXISTS ${dir})
                set(MRDOCS_TARGET_ADDONS ${dir})
                break()
            endif()
        endforeach()
    endif()

    if (NOT MRDOCS_TARGET_ADDONS)
        message(FATAL_ERROR "MrDocs: ADDONS directory not set and no addons found in ${CMAKE_CURRENT_SOURCE_DIR} or relative to ${MRDOCS_EXECUTABLE}")
    endif()

    #-------------------------------------------------
    # Comment
    #-------------------------------------------------
    if (NOT MRDOCS_TARGET_COMMENT)
        set(MRDOCS_TARGET_COMMENT "Generate ${CMAKE_PROJECT_NAME} documentation with MrDocs")
    endif()

    #-------------------------------------------------
    # Output
    #-------------------------------------------------
    if (NOT MRDOCS_TARGET_OUTPUT)
        set(MRDOCS_TARGET_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mrdocs)
    endif()

    #-------------------------------------------------
    # LibC++ paths
    #-------------------------------------------------
    if (NOT DEFINED MRDOCS_LIBCXX_PATHS)
        set(MRDOCS_LIBCXX_PATHS ${CMAKE_SOURCE_DIR}/share/libcxx)
    endif()

    #-------------------------------------------------
    # Custom target
    #-------------------------------------------------
    message(STATUS "MrDocs: Generating documentation for ${CMAKE_PROJECT_NAME} in ${MRDOCS_TARGET_OUTPUT}")
    set(MRDOCS_CMD_LINE_OPTIONS --config=${MRDOCS_TARGET_CONFIG} ${MRDOCS_COMPILE_COMMANDS}
            --addons=${MRDOCS_TARGET_ADDONS} --output=${MRDOCS_TARGET_OUTPUT})
    string(REPLACE ";" " " MRDOCS_WS_CMD_LINE_OPTIONS "${MRDOCS_CMD_LINE_OPTIONS}")
    message(STATUS "mrdocs ${MRDOCS_WS_CMD_LINE_OPTIONS}")
    add_custom_target(
            ${MRDOCS_TARGET_NAME}
            ${MRDOCS_TARGET_ALL_STR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MRDOCS_TARGET_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E echo "mrdocs ${MRDOCS_WS_CMD_LINE_OPTIONS}"
            COMMAND ${MRDOCS_EXECUTABLE} ${MRDOCS_CMD_LINE_OPTIONS}
            DEPENDS ${MRDOCS_TARGET_SOURCES} ${MRDOCS_TARGET_CONFIG} ${MRDOCS_COMPILE_COMMANDS} ${MRDOCS_TARGET_ADDONS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT ${MRDOCS_TARGET_COMMENT}
            USES_TERMINAL
    )
    if (MRDOCS_EXECUTABLE_DEPENDENCY)
        add_dependencies(${MRDOCS_TARGET_NAME} ${MRDOCS_EXECUTABLE_DEPENDENCY})
    endif()
endfunction()