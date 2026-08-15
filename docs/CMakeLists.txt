#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The documentation (Antora) build. This is a regular build: when
# MRDOCS_BUILD_DOCS is on we add custom targets that run mrdocs over its own
# headers (the reference) and Antora over the site. The separate "compile every
# public header" build is the MrDocs build (MRDOCS_MRDOCS_BUILD), and lives in
# docs/mrdocs-build.cmake (it compiles a different target and links nothing).

# ---- Reference (mrdocs run over its own headers) ----
if (MRDOCS_GENERATE_REFERENCE)
    set(MRDOCS_REFERENCE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/reference)
    file(GLOB_RECURSE REFERENCE_SOURCES CONFIGURE_DEPENDS
            ${PROJECT_SOURCE_DIR}/include/*.hpp ${PROJECT_SOURCE_DIR}/include/*.inc)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
    # Run the freshly built mrdocs over its own public headers. In-tree we point
    # it at the source-tree addons and the bundled libc++ / libc-stub headers
    # (LIBCXX_DIR comes from src/), so extraction does not depend on the host
    # toolchain. The config's compilation database drives which sources are read;
    # REFERENCE_SOURCES is a DEPENDS list so the target reruns when they change.
    add_custom_target(generate_reference ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MRDOCS_REFERENCE_OUTPUT_DIR}
            COMMAND $<TARGET_FILE:mrdocs>
                --config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
                --output=${MRDOCS_REFERENCE_OUTPUT_DIR}
                ${MRDOCS_BUILTIN_DIR_ARGS}
            DEPENDS ${REFERENCE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
            COMMENT "Generate MrDocs reference"
            USES_TERMINAL)
    add_dependencies(generate_reference mrdocs)
    if (MRDOCS_GENERATE_ANTORA_REFERENCE)
        set(MRDOCS_ANTORA_REFERENCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules/ROOT/pages/reference)
        add_custom_target(generate_antora_reference
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${MRDOCS_REFERENCE_OUTPUT_DIR} ${MRDOCS_ANTORA_REFERENCE_DIR}
                DEPENDS generate_reference
                COMMENT "Copy MrDocs reference to Antora")
    endif()
endif()

# ---- Exposition (the Antora site) ----
find_program(NPM_EXECUTABLE npm)
find_program(NPX_EXECUTABLE npx)

if(NPM_EXECUTABLE AND NPX_EXECUTABLE)
    message(STATUS "NPM found: ${NPM_EXECUTABLE}")
    message(STATUS "NPX found: ${NPX_EXECUTABLE}")

    set(DOCS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(generate_docs
            ALL
            COMMAND ${CMAKE_COMMAND} -E echo "Install npm dependencies"
            COMMAND ${NPM_EXECUTABLE} install
            COMMAND ${CMAKE_COMMAND} -E echo "Run Antora"
            COMMAND ${NPX_EXECUTABLE} antora antora-playbook.yml
            WORKING_DIRECTORY ${DOCS_SOURCE_DIR}
            COMMENT "Generating MrDocs documentation"
            USES_TERMINAL
            )
    if (MRDOCS_GENERATE_REFERENCE AND MRDOCS_GENERATE_ANTORA_REFERENCE)
        add_dependencies(generate_docs generate_antora_reference)
    endif()
else()
    message(WARNING "NPM/NPX not found. Unable to generate documentation.")
endif()
