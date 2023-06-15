#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
#
# Official repository: https://github.com/cppalliance/mrdox
#

macro(_mrdox_prep_vars)
    if (NOT DEFINED MRDOX_EXECUTABLE)
        set(MRDOX_EXECUTABLE $<TARGET_FILE:mrdox>)
        set(MRDOX_EXECUTABLE_DEPENDENCY mrdox)
    endif()

    if (NOT DEFINED MRDOX_COMPILE_COMMANDS)
        if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
            message(FATAL_ERROR "MrDox build script requires either CMAKE_EXPORT_COMPILE_COMMANDS=ON or MRDOX_COMPILE_COMMANDS to be set")
        else()
            set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        endif()
        set(MRDOX_COMPILE_COMMANDS ${CMAKE_BINARY_DIR}/compile_commands.json)
    endif()
endmacro()


function(mrdox)

    cmake_parse_arguments(
        MRDOX_TARGET
        "" # options
        "OUTPUT;FORMAT;CONFIG" # one_value_keywords
        "SOURCES"
        ${ARGN}
    )

    _mrdox_prep_vars()

    if (NOT MRDOX_TARGET_CONFIG)
        set(MRDOX_TARGET_CONFIG mrdox.yml)
    endif()

    if (NOT MRDOX_TARGET_OUTPUT)
        set(MRDOX_TARGET_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    if (NOT MRDOX_TARGET_FORMAT)
        set(MRDOX_TARGET_FORMAT xml)
    endif()

    set(MRDOX_OUTPUT_FILE ${MRDOX_TARGET_OUTPUT}/reference.${MRDOX_TARGET_FORMAT})
    add_custom_command(
            OUTPUT ${MRDOX_OUTPUT_FILE}
            COMMAND
                mrdox --config=${CMAKE_CURRENT_SOURCE_DIR}/${MRDOX_TARGET_CONFIG}
                        ${MRDOX_COMPILE_COMMANDS}
                        --addons=../addons
                        --format=${MRDOX_TARGET_FORMAT}
                        "--output=${MRDOX_TARGET_OUTPUT}"
            MAIN_DEPENDENCY ${MRDOX_TARGET_CONFIG} # scanner!
            DEPENDS ${MRDOX_EXECUTABLE_DEPENDENCY} ${MRDOX_TARGET_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()