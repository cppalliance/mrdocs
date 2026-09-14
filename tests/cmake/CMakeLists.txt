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

cmake_minimum_required(VERSION 3.20)
project(mrdocs_cmake_consumer LANGUAGES CXX C)

# Use mrdocs as a library and run it as a test. Discovery is driven by the
# exact-case mrdocs_ROOT (honored since CMake 3.12), so no policy opt-in is needed.
find_package(mrdocs REQUIRED)
add_executable(consumer src/main.cpp)
target_link_libraries(consumer PRIVATE mrdocs::mrdocs-core)
target_compile_features(consumer PRIVATE cxx_std_23)
enable_testing()
add_test(NAME consumer-runs COMMAND consumer)

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

if (MRDOCS_BUILT_PLUGIN_MODULES)
    add_library(consumer_plugin MODULE src/plugin.cpp)
    target_link_libraries(consumer_plugin PRIVATE mrdocs::mrdocs)
    target_compile_features(consumer_plugin PRIVATE cxx_std_23)

    set(plugin_addons "${CMAKE_CURRENT_BINARY_DIR}/plugin-addons")
    set_target_properties(consumer_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "$<1:${plugin_addons}/plugins>")

    string(REGEX MATCH "^[0-9]+\\.[0-9]+"
        mrdocs_built_cxx "${MRDOCS_BUILT_CXX_COMPILER_VERSION}")
    string(REGEX MATCH "^[0-9]+\\.[0-9]+"
        mrdocs_this_cxx "${CMAKE_CXX_COMPILER_VERSION}")
    if (MRDOCS_BUILT_CXX_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID AND
        mrdocs_built_cxx AND
        mrdocs_built_cxx VERSION_EQUAL mrdocs_this_cxx)
        add_test(NAME consumer-plugin-loads
            COMMAND ${MRDOCS_EXECUTABLE}
                --config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml
                --compilation-database=${CMAKE_CURRENT_SOURCE_DIR}/docs/compile_commands.json
                --output=${CMAKE_CURRENT_BINARY_DIR}/plugin-reference
                --addons-supplemental=${plugin_addons}
                --generator=consumer-probe)
    else ()
        message(STATUS
            "MrDocs was built with ${MRDOCS_BUILT_CXX_COMPILER_ID} "
            "${MRDOCS_BUILT_CXX_COMPILER_VERSION} and this project with "
            "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}: "
            "building the plugin, but not loading it")
    endif ()
endif ()
