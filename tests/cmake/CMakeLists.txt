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

    # The generator expression keeps CMake from appending a per-configuration
    # subdirectory, which would put the library somewhere the test cannot
    # name. Hidden visibility is what the Plugins page recommends.
    set(plugin_addons "${CMAKE_CURRENT_BINARY_DIR}/plugin-addons")
    set_target_properties(consumer_plugin PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        LIBRARY_OUTPUT_DIRECTORY "$<1:${plugin_addons}/plugins>")

    # MrDocs refuses a plugin built with another toolchain, so the plugin is
    # loaded only when this project's compiler is the one MrDocs was built
    # with; ci-releases.yml configures this project against a released MrDocs
    # with whatever compiler the runner has. The versions are compared up to
    # major.minor, which is what the toolchain tag records on MSVC and more
    # than it records for gcc and clang: a full version compare would skip
    # the test over patch levels the tag accepts.
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
