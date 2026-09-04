#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The MrDocs build (MRDOCS_MRDOCS_BUILD): compile a single translation unit that
# includes every public header, producing a compile_commands.json that covers the
# whole API so mrdocs can document itself. The root includes this file and returns
# right after, so none of the real-library CMake runs and nothing is linked. This
# is NOT the Antora docs build (MRDOCS_BUILD_DOCS).

# ---- Generated headers ----
# The public headers reference generated ones (e.g. Config.hpp includes
# <mrdocs/ConfigSchema.hpp>), so produce them now, at configure time, before the
# glob below picks them up. utils/codegen is not on the path in this build.
find_program(PYTHON_EXECUTABLE python3 python)
if (NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Python is needed to configure mrdocs")
endif()
find_package(Git QUIET)
execute_process(
        COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/utils/codegen/generate-version-header.py
                ${PROJECT_SOURCE_DIR}/include/mrdocs/Version.hpp.in
                ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
                --version ${PROJECT_VERSION}
                --name ${PROJECT_NAME}
                --description "${PROJECT_DESCRIPTION}"
                --source-dir ${PROJECT_SOURCE_DIR}
                --git "${GIT_EXECUTABLE}"
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)
execute_process(
        COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/utils/codegen/generate-config-info.py
                ${PROJECT_SOURCE_DIR}/src/mrdocs/ConfigOptions.json ${PROJECT_BINARY_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)
execute_process(
        COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/utils/codegen/generate-yaml-schema.py
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

# ---- mrdocs-reference ----
# One TU including every public header: mrdocs's own (source + generated) plus the
# dom and handlebars libraries it re-exports.
file(GLOB_RECURSE MRDOCS_REFERENCE_HEADERS
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_BINARY_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/libs/dom/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/libs/handlebars/include/*.hpp")

set(MRDOCS_REFERENCE_CPP "${PROJECT_BINARY_DIR}/mrdocs-reference.cpp")
file(WRITE "${MRDOCS_REFERENCE_CPP}" "// This file is generated automatically by CMake\n\n")
foreach (header IN LISTS MRDOCS_REFERENCE_HEADERS)
    file(TO_CMAKE_PATH "${header}" header)
    file(APPEND "${MRDOCS_REFERENCE_CPP}" "#include \"${header}\"\n")
endforeach ()

add_library(mrdocs-reference STATIC "${MRDOCS_REFERENCE_CPP}")
target_compile_features(mrdocs-reference PRIVATE cxx_std_23)
target_include_directories(mrdocs-reference PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}/include"
        "${PROJECT_SOURCE_DIR}/libs/polyfill/include"
        "${PROJECT_SOURCE_DIR}/libs/dom/include"
        "${PROJECT_SOURCE_DIR}/libs/handlebars/include")
target_compile_definitions(mrdocs-reference PRIVATE MRDOCS_STATIC_LINK)
# This target only needs the headers to compile, not to be warning-clean, so
# silence all warnings. The /Zc flags stay: they are needed to compile, not to
# quiet warnings (Platform.hpp gates on __cplusplus, Describe.hpp uses __VA_OPT__).
if (MSVC)
    target_compile_options(mrdocs-reference PRIVATE /Zc:__cplusplus /Zc:preprocessor /W0)
else ()
    target_compile_options(mrdocs-reference PRIVATE -w)
endif ()
