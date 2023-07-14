#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdox
#

#[=======================================================================[.rst:
MrDox
-----

This module provides a function to generate documentation from C++ source
files with mrdox.

It identifies the necessary MrDox configuration options for the current
project and generates a documentation target.

See the MrDox usage documentation for complete instructions.

#]=======================================================================]

function(add_mrdox MRDOX_TARGET_NAME)
    #-------------------------------------------------
    # Parse arguments
    #-------------------------------------------------
    set(options_prefix MRDOX_TARGET)
    set(options EXCLUDE_FROM_ALL)
    set(oneValueArgs CONFIG OUTPUT FORMAT ADDONS)
    set(multiValueArgs COMMENT)
    cmake_parse_arguments(${options_prefix} "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(MRDOX_TARGET_SOURCES ${MRDOX_TARGET_UNPARSED_ARGUMENTS})

    #-------------------------------------------------
    # Executable
    #-------------------------------------------------
    if (NOT DEFINED MRDOX_EXECUTABLE AND TARGET mrdox)
        set(MRDOX_EXECUTABLE $<TARGET_FILE:mrdox>)
        set(MRDOX_EXECUTABLE_DEPENDENCY mrdox)
    endif()
    if (NOT DEFINED MRDOX_EXECUTABLE)
        find_program(MRDOX_EXECUTABLE mrdox)
        if (NOT MRDOX_EXECUTABLE)
            message(FATAL_ERROR "MrDox build script requires mrdox to be installed")
        endif()
    endif()

    #-------------------------------------------------
    # CMake compile commands
    #-------------------------------------------------
    if (NOT DEFINED MRDOX_COMPILE_COMMANDS)
        if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
            message(FATAL_ERROR "MrDox requires either CMAKE_EXPORT_COMPILE_COMMANDS=ON or MRDOX_COMPILE_COMMANDS to be set")
        endif()
        set(MRDOX_COMPILE_COMMANDS ${CMAKE_BINARY_DIR}/compile_commands.json)
    endif()
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

    #-------------------------------------------------
    # EXCLUDE_FROM_ALL
    #-------------------------------------------------
    if (MRDOX_TARGET_EXCLUDE_FROM_ALL)
        set(MRDOX_TARGET_ALL_STR "")
    else()
        set(MRDOX_TARGET_ALL_STR "ALL")
    endif()

    #-------------------------------------------------
    # Configuration
    #-------------------------------------------------
    if (NOT MRDOX_TARGET_CONFIG)
        set(MRDOX_TARGET_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/mrdox.yml)
        if (NOT EXISTS ${MRDOX_TARGET_CONFIG})
            foreach (dir doc docs antora)
                if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdox.yml)
                    set(MRDOX_TARGET_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdox.yml)
                    break()
                endif()
            endforeach()
        endif()
        if (NOT EXISTS ${MRDOX_TARGET_CONFIG})
            foreach (dir ${CMAKE_CURRENT_SOURCE_DIR})
                if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdox.yml)
                    set(MRDOX_TARGET_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mrdox.yml)
                    break()
                endif()
            endforeach()
        endif()
        if (NOT EXISTS ${MRDOX_TARGET_CONFIG})
            message(FATAL_ERROR "MrDox: CONFIG option not set and no mrdox.yml found in ${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
    endif()
    get_filename_component(MRDOX_TARGET_CONFIG ${MRDOX_TARGET_CONFIG} ABSOLUTE)

    #-------------------------------------------------
    # Format
    #-------------------------------------------------
    if (NOT MRDOX_TARGET_FORMAT)
        set(MRDOX_TARGET_FORMAT adoc)
    endif()

    #-------------------------------------------------
    # Format
    #-------------------------------------------------
    if (MRDOX_TARGET_ADDONS)
        if (NOT EXISTS ${MRDOX_TARGET_ADDONS})
            message(FATAL_ERROR "MrDox: ADDONS directory ${MRDOX_TARGET_ADDONS} does not exist")
        endif()
    endif()

    if (NOT MRDOX_TARGET_ADDONS)
        get_filename_component(MRDOX_EXECUTABLE_DIR ${MRDOX_EXECUTABLE} DIRECTORY)
        set(${PROJECT_NAME}_DIR ${CMAKE_CURRENT_SOURCE_DIR})
        set(DEFAULT_ADDONS_PATHS
            # Project has its own addons
            ${${PROJECT_NAME}_DIR}/share/mrdox/addons
            ${${PROJECT_NAME}_DIR}/docs/mrdox/addons
            ${${PROJECT_NAME}_DIR}/docs/addons
            ${${PROJECT_NAME}_DIR}/antora/addons
            # Relative to mrdox executable
            ${MRDOX_EXECUTABLE_DIR}/../share/mrdox/addons # FHS
            ${MRDOX_EXECUTABLE_DIR}/share/mrdox/addons    # FHS with no `bin`
            ${MRDOX_EXECUTABLE_DIR}/../addons             # Non-FHS
            ${MRDOX_EXECUTABLE_DIR}/addons                # Non-FHS with no `bin`
        )
        foreach (dir ${DEFAULT_ADDONS_PATHS})
            message(STATUS "MrDox: Looking for addons in ${dir}")
            if (EXISTS ${dir})
                set(MRDOX_TARGET_ADDONS ${dir})
                break()
            endif()
        endforeach()
    endif()

    if (NOT MRDOX_TARGET_ADDONS)
        message(FATAL_ERROR "MrDox: ADDONS directory not set and no addons found in ${CMAKE_CURRENT_SOURCE_DIR} or relative to ${MRDOX_EXECUTABLE}")
    endif()

    #-------------------------------------------------
    # Comment
    #-------------------------------------------------
    if (NOT MRDOX_TARGET_COMMENT)
        set(MRDOX_TARGET_COMMENT "Generate ${CMAKE_PROJECT_NAME} documentation with MrDox")
    endif()

    #-------------------------------------------------
    # Output
    #-------------------------------------------------
    if (NOT MRDOX_TARGET_OUTPUT)
        set(MRDOX_TARGET_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mrdox/${MRDOX_TARGET_FORMAT})
    endif()


    #-------------------------------------------------
    # Custom target
    #-------------------------------------------------
    message(STATUS "MrDox: Generating documentation for ${CMAKE_PROJECT_NAME} in ${MRDOX_TARGET_OUTPUT}")
    set(MRDOX_CMD_LINE_OPTIONS --config=${MRDOX_TARGET_CONFIG} ${MRDOX_COMPILE_COMMANDS}
            --addons=${MRDOX_TARGET_ADDONS} --format=${MRDOX_TARGET_FORMAT} --output=${MRDOX_TARGET_OUTPUT})
    string(REPLACE ";" " " MRDOX_WS_CMD_LINE_OPTIONS "${MRDOX_CMD_LINE_OPTIONS}")
    message(STATUS "mrdox ${MRDOX_WS_CMD_LINE_OPTIONS}")
    add_custom_target(
            ${MRDOX_TARGET_NAME}
            ${MRDOX_TARGET_ALL_STR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MRDOX_TARGET_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E echo "mrdox ${MRDOX_WS_CMD_LINE_OPTIONS}"
            COMMAND ${MRDOX_EXECUTABLE} ${MRDOX_CMD_LINE_OPTIONS}
            DEPENDS ${MRDOX_TARGET_SOURCES} ${MRDOX_TARGET_CONFIG} ${MRDOX_COMPILE_COMMANDS} ${MRDOX_TARGET_ADDONS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT ${MRDOX_TARGET_COMMENT}
            USES_TERMINAL
    )
    if (MRDOX_EXECUTABLE_DEPENDENCY)
        add_dependencies(${MRDOX_TARGET_NAME} ${MRDOX_EXECUTABLE_DEPENDENCY})
    endif()
endfunction()