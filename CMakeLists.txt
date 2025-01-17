#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

#-------------------------------------------------
#
# Project settings
#
#-------------------------------------------------

cmake_minimum_required(VERSION 3.13)
if (GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)
elseif(NOT GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()
cmake_policy(SET CMP0111 OLD)
project(
    MrDocs
    VERSION 0.0.3
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdocs"
    LANGUAGES CXX C
)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(MRDOCS_INSTALL "Configure install target" ON)
option(MRDOCS_PACKAGE "Build install package" ON)
option(MRDOCS_BUILD_SHARED "Link shared" ${BUILD_SHARED_LIBS})
option(MRDOCS_BUILD_TESTS "Build tests" ${BUILD_TESTING})
option(MRDOCS_REQUIRE_GIT "Git is required: not being able to extract version build is an error" ON)
if (MRDOCS_BUILD_TESTS OR MRDOCS_INSTALL)
    option(MRDOCS_BUILD_DOCS "Build documentation" ON)
else()
    option(MRDOCS_BUILD_DOCS "Build documentation" OFF)
endif()
option(MRDOCS_GENERATE_REFERENCE "Generate MrDocs reference" ${MRDOCS_BUILD_DOCS})
option(MRDOCS_GENERATE_ANTORA_REFERENCE "Generate MrDocs reference in Antora module pages" OFF)

if (MRDOCS_BUILD_SHARED)
    set(MRDOCS_LINK_MODE SHARED)
    set(MRDOCS_LINK_MODE_DEFINITION -DMRDOCS_SHARED_LINK)
else()
    set(MRDOCS_LINK_MODE_DEFINITION -DMRDOCS_STATIC_LINK)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(MRDOCS_GCC ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
    set(MRDOCS_CLANG ON)
endif()

#-------------------------------------------------
#
# Dependencies
#
#-------------------------------------------------

set(CMAKE_FOLDER Dependencies)

# LLVM + Clang
if (LLVM_ROOT)
    # LLVM_ROOT is absolute
    get_filename_component(LLVM_ROOT "${LLVM_ROOT}" ABSOLUTE)
    set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "Root of LLVM install." FORCE)
    if (NOT EXISTS "${LLVM_ROOT}")
        message(FATAL_ERROR
               "LLVM_ROOT (${LLVM_ROOT}) provided does not exist.\n"
                "Please set LLVM_ROOT to the root of LLVM install.\n")
    endif()
    if (NOT EXISTS "${LLVM_ROOT}/lib/cmake/llvm")
        message(FATAL_ERROR
                "LLVM_ROOT (${LLVM_ROOT}) provided is invalid.\n"
                "No <LLVM_ROOT>/lib/cmake/llvm found.\n"
                "Please set LLVM_ROOT to the root of LLVM install.\n")
    endif()
    message(STATUS "LLVM_ROOT: ${LLVM_ROOT}")
endif()
if (Clang_ROOT)
    # Clang_ROOT is absolute
    get_filename_component(Clang_ROOT "${Clang_ROOT}" ABSOLUTE)
    set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "Root of Clang install." FORCE)
elseif (LLVM_ROOT)
    # Clang_ROOT matches LLVM_ROOT by default
    set(Clang_ROOT "${LLVM_ROOT}" CACHE PATH "Root of Clang install." FORCE)
endif()
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

if (LLVM_ROOT)
    set(LIBCXX_DIR "${LLVM_ROOT}/include/c++/v1/")
    message(STATUS "LIBCXX_DIR: ${LIBCXX_DIR}")
    if (NOT EXISTS "${LIBCXX_DIR}")
        message(FATAL_ERROR
                "LIBCXX_DIR (${LIBCXX_DIR}) does not exist.\n"
                "LLVM_ROOT is set, but LIBCXX_DIR (/include/c++/v1) does not exist.\n")
    endif()
    set(STDLIB_INCLUDE_DIR "${LLVM_ROOT}/lib/clang/${Clang_VERSION_MAJOR}/include/")
    message(STATUS "STDLIB_INCLUDE_DIR: ${STDLIB_INCLUDE_DIR}")
    if (NOT EXISTS "${STDLIB_INCLUDE_DIR}")
        message(FATAL_ERROR
                "STDLIB_INCLUDE_DIR (${STDLIB_INCLUDE_DIR}) does not exist.\n"
                "LLVM_ROOT is set, but STDLIB_INCLUDE_DIR (/lib/clang/${Clang_VERSION_MAJOR}/include) does not exist.\n")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs all)
string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# fmt
find_package(fmt REQUIRED CONFIG)

# Duktape
find_package(Duktape CONFIG)
if (NOT DUKTAPE_FOUND)
    # Duktape doesn't nativelly support CMake.
    # Some config script patches use the capitalized version.
    find_package(duktape REQUIRED CONFIG)
endif()

# python
find_program(PYTHON_EXECUTABLE python3 python)
if (NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Python is needed to configure mrdocs")
endif()

unset(CMAKE_FOLDER)

#-------------------------------------------------
#
# mrdocs-core
#
#-------------------------------------------------
# Create include/mrdocs/Version.hpp from Version.hpp.in
find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE PROJECT_VERSION_BUILD
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Check if PROJECT_VERSION_BUILD is populated
    if (NOT PROJECT_VERSION_BUILD AND MRDOCS_REQUIRE_GIT)
        message(FATAL_ERROR "Git was found but could not extract the version build")
    endif()
    set(PROJECT_VERSION_BUILD "${PROJECT_VERSION_BUILD}")
else()
    if (MRDOCS_REQUIRE_GIT)
        message(FATAL_ERROR "Git is required to extract the version build")
    endif()
    set(PROJECT_VERSION_BUILD "")
endif()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mrdocs/Version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Version.hpp
    @ONLY
)

# Config files for mrdocs-core
add_custom_command(
    COMMAND
        ${PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/util/generate-config-info.py
        src/lib/Lib/ConfigOptions.json
        ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
        ${PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/util/generate-yaml-schema.py
    VERBATIM
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/util/generate-config-info.py
        src/lib/Lib/ConfigOptions.json
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/PublicSettings.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/src/lib/Lib/PublicSettings.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/src/tool/PublicToolArgs.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/src/tool/PublicToolArgs.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Config Source Files"
)

add_custom_command(
    COMMAND
        ${PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/util/generate-info-files.py
        src/lib/Metadata/InfoNodes.json
        ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/util/generate-info-files.py
        src/lib/Metadata/InfoNodes.json
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesLower.inc
        ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesPascal.inc
        ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesPascalAndLower.inc
        ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesPascalPlural.inc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Info Include Files"
)


# Main library
file(
    GLOB_RECURSE LIB_SOURCES  CONFIGURE_DEPENDS
        # Public
        include/*.hpp
        include/*.inc
        include/*.ipp
        # Private
        src/lib/*.cpp
        src/lib/*.hpp
        src/lib/*.ipp
        src/lib/*.natvis
        # Natvis
        include/*.natvis
        SourceFileNames.cpp
)
list(APPEND LIB_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Version.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/PublicSettings.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/src/lib/Lib/PublicSettings.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesLower.inc
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesPascal.inc
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesPascalAndLower.inc
    ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs/Metadata/InfoNodesPascalPlural.inc
)
add_library(mrdocs-core ${LIB_SOURCES})
target_compile_features(mrdocs-core PUBLIC cxx_std_20)
target_include_directories(mrdocs-core
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src")
target_compile_definitions(
    mrdocs-core
    PUBLIC
    ${MRDOCS_LINK_MODE_DEFINITION}
    PRIVATE
    -DMRDOCS_TOOL
)

# Dependencies
target_include_directories(mrdocs-core SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(mrdocs-core SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})
target_link_libraries(mrdocs-core PUBLIC fmt::fmt)
target_include_directories(mrdocs-core SYSTEM PRIVATE ${DUKTAPE_INCLUDE_DIRS})
target_link_libraries(mrdocs-core PRIVATE ${DUKTAPE_LIBRARY})

# Clang
if (CLANG_SIMPLE_LIBS)
    target_link_libraries(mrdocs-core PUBLIC LLVM clang clang-cpp)
else()
    target_link_libraries(mrdocs-core
        PUBLIC
            clangAST
            clangBasic
            clangFrontend
            clangIndex
            clangTooling
            clangToolingCore
            clangToolingInclusions)
endif()

# Windows, Win64
if (WIN32)
    target_compile_definitions(
        mrdocs-core
        PUBLIC
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
        -DFMT_HEADER_ONLY # because of _ITERATOR_DEBUG_LEVEL
    )
    if(MSVC)
        get_target_property(LLVM_CONFIGURATION_TYPE LLVMCore IMPORTED_CONFIGURATIONS)
        if (LLVM_CONFIGURATION_TYPE STREQUAL RELWITHDEBINFO)
            # Handle Debug/RelWithDebInfo mismatch between mrdocs and LLVM
            target_compile_definitions(mrdocs-core PUBLIC $<$<CONFIG:Debug>:-D_ITERATOR_DEBUG_LEVEL=0>)
            target_compile_options(mrdocs-core PUBLIC $<$<CONFIG:Debug>:/MD>)
        endif()
        if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64") # 64-bit
            target_compile_options(mrdocs-core PUBLIC /arch:AMD64)
        endif()
        target_compile_options(
            mrdocs-core
            PUBLIC
            /permissive-    # strict C++
            /W4             # enable all warnings
            /MP             # multi-processor compilation
            /EHs            # C++ Exception handling
            $<$<CONFIG:Debug>:/Oy-> # Disable frame pointer omission
        )
    endif()
endif ()

if (MRDOCS_CLANG)
    target_compile_options(
        mrdocs-core
        PUBLIC
        -Wno-unused-private-field
        -Wno-unused-value
        PRIVATE
        -Wno-covered-switch-default
    )
endif ()

#-------------------------------------------------
#
# Tool
#
#-------------------------------------------------

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS
        src/tool/*.cpp
        src/tool/*.hpp)
list(APPEND TOOL_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/src/tool/PublicToolArgs.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/src/tool/PublicToolArgs.cpp)

add_executable(mrdocs ${TOOL_SOURCES})
target_compile_definitions(mrdocs PRIVATE -DMRDOCS_TOOL)

target_include_directories(mrdocs
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src"
)

target_compile_definitions(mrdocs PRIVATE -DMRDOCS_TOOL)
target_link_libraries(mrdocs PUBLIC mrdocs-core)
if (MRDOCS_CLANG)
    target_compile_options(
        mrdocs
        PRIVATE
        -Wno-covered-switch-default
    )
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${PROJECT_SOURCE_DIR}/include/mrdocs PREFIX "include" FILES ${INCLUDES})
source_group(TREE ${PROJECT_SOURCE_DIR}/source PREFIX "source" FILES ${SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

if (MRDOCS_BUILD_TESTS)
    #-------------------------------------------------
    # Unit tests
    #-------------------------------------------------
    include(CTest)
    file(GLOB_RECURSE TEST_SUITE_FILES CONFIGURE_DEPENDS src/test_suite/*.cpp src/test_suite/*.hpp)
    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS src/test/*.cpp src/test/*.hpp)
    add_executable(mrdocs-test ${TEST_SUITE_FILES} ${UNIT_TEST_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/src/tool/PublicToolArgs.cpp)
    target_include_directories(mrdocs-test
            PUBLIC
            "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
            PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_BINARY_DIR}/src"
            )
    target_link_libraries(mrdocs-test PUBLIC mrdocs-core)
    if (MRDOCS_CLANG)
        target_compile_options(mrdocs-test PRIVATE -Wno-covered-switch-default)
    endif ()
    target_compile_definitions(mrdocs-test PRIVATE -DMRDOCS_TEST_FILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test-files")
    add_test(NAME mrdocs-unit-tests COMMAND mrdocs-test --unit=true)
    foreach (testgenerator IN ITEMS xml adoc html)
        add_test(NAME mrdocs-golden-tests-${testgenerator}
            COMMAND
                mrdocs-test
                --unit=false
                --action=test
                "${PROJECT_SOURCE_DIR}/test-files/golden-tests"
                "--addons=${CMAKE_SOURCE_DIR}/share/mrdocs/addons"
                --generator=${testgenerator}
                "--stdlib-includes=${LIBCXX_DIR}"
                "--stdlib-includes=${STDLIB_INCLUDE_DIR}"
                "--libc-includes=${CMAKE_SOURCE_DIR}/share/mrdocs/headers/libc-stubs"
                --report=2
        )
        foreach (action IN ITEMS test create update)
            add_custom_target(
                mrdocs-${action}-test-fixtures-${testgenerator}
                COMMAND
                    mrdocs-test
                    --unit=false
                    --action=${action}
                    "${PROJECT_SOURCE_DIR}/test-files/golden-tests"
                    "--addons=${CMAKE_SOURCE_DIR}/share/mrdocs/addons"
                    --generator=${testgenerator}
                    "--stdlib-includes=${LIBCXX_DIR}"
                    "--stdlib-includes=${STDLIB_INCLUDE_DIR}"
                    "--libc-includes=${CMAKE_SOURCE_DIR}/share/mrdocs/headers/libc-stubs"
                    --report=2
                DEPENDS mrdocs-test
            )
        endforeach ()
    endforeach()

    #-------------------------------------------------
    # XML lint
    #-------------------------------------------------
    find_package(LibXml2)
    if (LibXml2_FOUND)
        find_package(Java REQUIRED Runtime)
        # FindJava
        if (NOT Java_FOUND)
            message(FATAL_ERROR "Java is needed to run xml-lint")
        endif()

        add_custom_command(
            COMMAND ${Java_JAVA_EXECUTABLE} -jar ${CMAKE_CURRENT_SOURCE_DIR}/util/trang.jar
                ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.rnc ${CMAKE_CURRENT_BINARY_DIR}/mrdocs.rng
            OUTPUT mrdocs.rng
            DEPENDS mrdocs.rnc)
        add_custom_target(mrdocs_rng ALL DEPENDS mrdocs.rng)

        file(GLOB_RECURSE XML_SOURCES CONFIGURE_DEPENDS test-files/golden-tests/*.xml)
        add_test(NAME xml-lint
                 COMMAND ${LIBXML2_XMLLINT_EXECUTABLE} --dropdtd --noout
                     --relaxng ${CMAKE_CURRENT_BINARY_DIR}/mrdocs.rng ${XML_SOURCES}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()

#-------------------------------------------------
#
# Docs
#
#-------------------------------------------------

if (MRDOCS_BUILD_DOCS)
    #-------------------------------------------------
    # Reference
    #-------------------------------------------------
    if (MRDOCS_GENERATE_REFERENCE)
        include(share/cmake/MrDocs.cmake)
        set(MRDOCS_REFERENCE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/reference)
        file(GLOB_RECURSE REFERENCE_SOURCES CONFIGURE_DEPENDS include/*.hpp include/*.inc)
        set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        add_mrdocs(generate_reference
                CONFIG docs/mrdocs.yml
                COMMENT "Generate MrDocs reference"
                OUTPUT ${MRDOCS_REFERENCE_OUTPUT_DIR}
                ${REFERENCE_SOURCES})
        if (MRDOCS_GENERATE_ANTORA_REFERENCE)
            set(MRDOCS_ANTORA_REFERENCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/modules/ROOT/pages/reference)
            add_custom_target(generate_antora_reference
                    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MRDOCS_REFERENCE_OUTPUT_DIR} ${MRDOCS_ANTORA_REFERENCE_DIR}
                    DEPENDS generate_reference
                    COMMENT "Copy MrDocs reference to Antora")
        endif()
    endif()

    #-------------------------------------------------
    # Exposition
    #-------------------------------------------------
    find_program(NPM_EXECUTABLE npm)
    find_program(NPX_EXECUTABLE npx)

    if(NPM_EXECUTABLE AND NPX_EXECUTABLE)
        message(STATUS "NPM found: ${NPM_EXECUTABLE}")
        message(STATUS "NPX found: ${NPX_EXECUTABLE}")

        set(DOCS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs)
        set(DOCS_BUILD_DIR ${DOCS_SOURCE_DIR}/build/site)
        set(DOCS_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/doc/mrdocs/html)

        # Add custom target for generating documentation
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
    elseif (NOT NPM_EXECUTABLE AND NPX_EXECUTABLE)
        message(WARNING "NPM not found. Unable to generate documentation.")
    elseif (NPM_EXECUTABLE AND NOT NPX_EXECUTABLE)
        message(WARNING "NPX not found. Unable to generate documentation.")
    else()
        message(WARNING "NPM and NPX not found. Unable to generate documentation.")
    endif()
endif ()

#-------------------------------------------------
#
# Install
#
#-------------------------------------------------

if (MRDOCS_INSTALL)
    #-------------------------------------------------
    # bin
    #-------------------------------------------------
    install(TARGETS mrdocs
            EXPORT mrdocs-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
    set(MRDOCS_EXECUTABLE $<TARGET_FILE:mrdocs>)

    #-------------------------------------------------
    # lib
    #-------------------------------------------------
    install(TARGETS mrdocs-core
            EXPORT mrdocs-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT development
            )

    install(EXPORT mrdocs-targets
            FILE mrdocs-targets.cmake
            NAMESPACE mrdocs::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    #-------------------------------------------------
    # include
    #-------------------------------------------------
    foreach(MRDOCS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/mrdocs ${CMAKE_CURRENT_BINARY_DIR}/include/mrdocs)
        foreach(MRDOCS_FILE_PATTERN "*.hpp" "*.ipp" "*.inc")
            install(DIRECTORY ${MRDOCS_INCLUDE_DIR}
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                    COMPONENT development
                    FILES_MATCHING
                    PATTERN ${MRDOCS_FILE_PATTERN})
        endforeach()
    endforeach()

    #-------------------------------------------------
    # share
    #-------------------------------------------------
    install(DIRECTORY ${LIBCXX_DIR}
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs/headers/libcxx
            FILES_MATCHING PATTERN "*")
    install(DIRECTORY ${STDLIB_INCLUDE_DIR}
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs/headers/clang
            FILES_MATCHING PATTERN "*")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/mrdocs/headers/libc-stubs/
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs/headers/libc-stubs
            FILES_MATCHING PATTERN "*")

    foreach (share_mrdocs_dir addons)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share/mrdocs/${share_mrdocs_dir}
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs
                FILES_MATCHING PATTERN "*")
    endforeach ()
    foreach (share_dir cmake gdb)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share/${share_dir}
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs
                FILES_MATCHING PATTERN "*")
    endforeach ()

    #-------------------------------------------------
    # doc
    #-------------------------------------------------
    if (MRDOCS_BUILD_DOCS AND NPM_EXECUTABLE AND NPX_EXECUTABLE)
        if (EXISTS ${DOCS_BUILD_DIR})
            install(DIRECTORY ${DOCS_BUILD_DIR}
                    DESTINATION ${CMAKE_INSTALL_DOCDIR}
                    COMPONENT documentation
                    )
        endif()
    endif()

    #-------------------------------------------------
    # lib/cmake
    #-------------------------------------------------
    set(CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    # mrdocs-config-version.cmake
    if (CMAKE_PROJECT_VERSION VERSION_LESS 1.0.0)
        set(compatibility_mode SameMajorVersion)
    else ()
        set(compatibility_mode SameMinorVersion)
    endif ()
    write_basic_package_version_file(
            mrdocs-config-version.cmake
            VERSION ${PACKAGE_VERSION}
            COMPATIBILITY ${compatibility_mode})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config-version.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    # mrdocs-config.cmake
    set(INCLUDE_INSTALL_DIR include/)
    set(LIB_INSTALL_DIR lib/)
    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs-config.cmake.in # input file
            ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config.cmake    # output file
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs
            PATH_VARS CMAKE_INSTALL_LIBDIR INCLUDE_INSTALL_DIR LIB_INSTALL_DIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)
endif ()

#-------------------------------------------------
#
# Packages
#
#-------------------------------------------------
if (MRDOCS_INSTALL AND MRDOCS_PACKAGE)
    # Project description
    set(CPACK_PACKAGE_VENDOR "mrdocs")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.adoc")

    # Ignore files
    FILE(READ .gitignore GITIGNORE_CONTENTS)
    STRING(REGEX REPLACE ";" "\\\\;" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    STRING(REGEX REPLACE "\n" ";" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    set(CPACK_SOURCE_IGNORE_FILES ${GITIGNORE_CONTENTS})

    # CPack variables
    include(CPack)
endif ()
