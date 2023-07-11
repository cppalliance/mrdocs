#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdox
#

#-------------------------------------------------
#
# Project settings
#
#-------------------------------------------------

cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0111 OLD)
project(
    MrDox
    VERSION 1.0.0
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdox"
    LANGUAGES CXX C
)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(MRDOX_BUILD_TESTS "Build tests" ${BUILD_TESTING})
option(MRDOX_BUILD_SHARED "Link shared" OFF)
option(MRDOX_BUILD_DOCS "Build documentation" OFF)
option(MRDOX_INSTALL "Configure install target" ON)
option(MRDOX_PACKAGE "Build install package" ON)
option(MRDOX_GENERATE_REFERENCE "Generate reference.xml/reference.adoc" ON)

if (MRDOX_BUILD_SHARED)
    set(MRDOX_LINK_MODE SHARED)
    set(MRDOX_LINK_MODE_DEFINITION -DMRDOX_SHARED_LINK)
else()
    set(MRDOX_LINK_MODE_DEFINITION -DMRDOX_STATIC_LINK)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(MRDOX_GCC ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
    set(MRDOX_CLANG ON)
endif()

if (WIN32)
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING)
    if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64") # 64-bit
        add_compile_options(
            /arch:AMD64
        )
    endif()
endif()

#-------------------------------------------------
#
# Dependencies
#
#-------------------------------------------------

set(CMAKE_FOLDER Dependencies)

# LLVM
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs all)
string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Duktape
if(NOT DEFINED DUKTAPE_SOURCE_ROOT)
    set(DUKTAPE_SOURCE_ROOT $ENV{DUKTAPE_SOURCE_ROOT})
endif()
if (NOT DUKTAPE_SOURCE_ROOT)
    include(FetchContent)
    if (POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
            duktape
            URL https://github.com/svaarala/duktape/releases/download/v2.7.0/duktape-2.7.0.tar.xz
    )
    FetchContent_GetProperties(duktape)
    if(NOT duktape_POPULATED)
        FetchContent_Populate(duktape)
        set(DUKTAPE_SOURCE_ROOT ${duktape_SOURCE_DIR})
    endif()
endif()
add_library(duktape ${DUKTAPE_SOURCE_ROOT}/src/duktape.c ${DUKTAPE_SOURCE_ROOT}/src/duktape.h)
target_include_directories(duktape PUBLIC ${DUKTAPE_SOURCE_ROOT}/src)
add_library(duktape::duktape ALIAS duktape)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(duktape PRIVATE -w)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(duktape PRIVATE /w)
endif()

# fmt
unset(CMAKE_FOLDER)
find_package(fmt REQUIRED CONFIG)

#-------------------------------------------------
#
# mrdox-core
#
#-------------------------------------------------

file(
    GLOB_RECURSE LIB_SOURCES  CONFIGURE_DEPENDS
        src/lib/*.cpp
        src/lib/*.hpp
        src/lib/*.natvis
        include/*.hpp
        include/*.natvis
        SourceFileNames.cpp)
add_library(mrdox-core ${LIB_SOURCES})
target_compile_features(mrdox-core PUBLIC cxx_std_20)
target_include_directories(mrdox-core
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(
    mrdox-core
    PUBLIC
    ${MRDOX_LINK_MODE_DEFINITION}
    PRIVATE
    -DMRDOX_TOOL
)

# Deps
target_include_directories(mrdox-core SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(mrdox-core SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})
target_link_libraries(mrdox-core PUBLIC fmt::fmt duktape::duktape)

# Clang
if (CLANG_SIMPLE_LIBS)
    target_link_libraries(mrdox-core PUBLIC LLVM clang clang-cpp)
else()
    target_link_libraries(mrdox-core
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
        mrdox-core
        PUBLIC
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
        -DFMT_HEADER_ONLY # because of _ITERATOR_DEBUG_LEVEL
    )
    get_target_property(LLVM_CONFIGURATION_TYPE LLVMCore IMPORTED_CONFIGURATIONS)
    if (LLVM_CONFIGURATION_TYPE STREQUAL RELWITHDEBINFO)
        target_compile_definitions(mrdox-core PUBLIC -D_ITERATOR_DEBUG_LEVEL=0)
        target_compile_options(mrdox-core PUBLIC /MD)
    endif()
    if(MSVC)
        target_compile_options(
            mrdox-core
            PUBLIC
            /permissive-    # strict C++
            /W4             # enable all warnings
            /MP             # multi-processor compilation
            /EHs            # C++ Exception handling
        )
    endif()
endif ()

if (MRDOX_CLANG)
    target_compile_options(
        mrdox-core
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

file(GLOB_RECURSE TOOL_SOURCES  CONFIGURE_DEPENDS src/tool/*.cpp src/tool/*.hpp)
add_executable(mrdox ${TOOL_SOURCES})
target_compile_definitions(mrdox PRIVATE -DMRDOX_TOOL)

target_include_directories(mrdox
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
)

target_compile_definitions(mrdox PRIVATE -DMRDOX_TOOL)
target_link_libraries(mrdox PUBLIC mrdox-core)
if (MRDOX_CLANG)
    target_compile_options(
        mrdox
        PRIVATE
        -Wno-covered-switch-default
    )
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${PROJECT_SOURCE_DIR}/include/mrdox PREFIX "include" FILES ${INCLUDES})
source_group(TREE ${PROJECT_SOURCE_DIR}/source PREFIX "source" FILES ${SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

if (MRDOX_BUILD_TESTS)
    #-------------------------------------------------
    # Unit tests
    #-------------------------------------------------
    include(CTest)
    file(GLOB_RECURSE TEST_SUITE_FILES CONFIGURE_DEPENDS src/test/test_suite/*.cpp src/test/test_suite/*.hpp)
    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS src/test/unit/*.cpp src/test/unit/*.hpp)
    add_executable(mrdox-test ${TEST_SUITE_FILES} ${UNIT_TEST_SOURCES})
    target_include_directories(mrdox-test
            PRIVATE
            "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_SOURCE_DIR}/src"
            )
    target_link_libraries(mrdox-test PUBLIC mrdox-core)
    if (MRDOX_CLANG)
        target_compile_options(mrdox-test PRIVATE -Wno-covered-switch-default)
    endif ()
    target_compile_definitions(mrdox-test PRIVATE -DMRDOX_TEST_FILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test-files")
    add_custom_command(
            # if we run tests, we need the addons in the right place.
            TARGET mrdox-test
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${CMAKE_SOURCE_DIR}/addons
                    ${CMAKE_BINARY_DIR}/addons
            BYPRODUCTS ${CMAKE_BINARY_DIR}/addons
            DEPENDS ${CMAKE_SOURCE_DIR}/addons
    )
    add_test(NAME mrdox-test COMMAND mrdox-test --action test "${PROJECT_SOURCE_DIR}/test-files/old-tests")

    #-------------------------------------------------
    # Reference documentation
    #-------------------------------------------------
    if (MRDOX_GENERATE_REFERENCE)
        # test run
        file(GLOB_RECURSE REFERENCE_SOURCES CONFIGURE_DEPENDS source/*.cpp source/*.hpp)

        set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        include(mrdox.cmake)

        mrdox(FORMAT adoc CONFIG docs/mrdox.yml SOURCES ${REFERENCE_SOURCES})
        mrdox(FORMAT xml  CONFIG docs/mrdox.yml SOURCES ${REFERENCE_SOURCES})

        add_custom_target(reference_adoc ALL DEPENDS reference.adoc)
        add_custom_target(reference_xml  ALL DEPENDS reference.xml)
    endif()

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
                ${CMAKE_CURRENT_SOURCE_DIR}/mrdox.rnc ${CMAKE_CURRENT_BINARY_DIR}/mrdox.rng
            OUTPUT mrdox.rng
            DEPENDS mrdox.rnc)
        add_custom_target(mrdox_rng ALL DEPENDS mrdox.rng)

        file(GLOB_RECURSE XML_SOURCES CONFIGURE_DEPENDS test-files/*.xml)
        add_test(NAME xml-lint
                 COMMAND ${LIBXML2_XMLLINT_EXECUTABLE} --dropdtd --noout
                     --relaxng ${CMAKE_CURRENT_BINARY_DIR}/mrdox.rng ${XML_SOURCES}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

endif()


#-------------------------------------------------
#
# Docs
#
#-------------------------------------------------

if (MRDOX_BUILD_DOCS)
    find_program(NPM_EXECUTABLE npm)
    find_program(NPX_EXECUTABLE npx)

    if(NPM_EXECUTABLE AND NPX_EXECUTABLE)
        message(STATUS "NPM found: ${NPM_EXECUTABLE}")
        message(STATUS "NPX found: ${NPX_EXECUTABLE}")

        set(DOCS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs)
        set(DOCS_BUILD_DIR ${DOCS_SOURCE_DIR}/build/site)
        set(DOCS_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/doc/mrdox/html)

        # Add custom target for generating documentation
        add_custom_target(generate_docs
                COMMAND ${NPM_EXECUTABLE} install
                COMMAND ${NPX_EXECUTABLE} antora local-antora-playbook.yml
                WORKING_DIRECTORY ${DOCS_SOURCE_DIR}
                COMMENT "Generating MrDox documentation"
                )
    else()
        message(WARNING "NPM or NPX not found. Unable to generate documentation.")
    endif()
endif ()

#-------------------------------------------------
#
# Install
#
#-------------------------------------------------

if (MRDOX_INSTALL)
    # Create and install mrdox-targets.cmake
    install(TARGETS mrdox
            EXPORT mrdox-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            )

    install(EXPORT mrdox-targets
            FILE mrdox-targets.cmake
            NAMESPACE mrdox::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdox)

    # Headers
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/mrdox
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.[hic]pp")

    # Set installation rules for generated documentation
    if (MRDOX_BUILD_DOCS)
        install(DIRECTORY ${DOCS_BUILD_DIR}
                DESTINATION ${DOCS_INSTALL_DIR}
                COMPONENT documentation
                )
    endif()

    # Set variable where the cmake config is
    # https://cliutils.gitlab.io/modern-cmake/chapters/install/installing.html
    set(CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/mrdox)

    # Create and install mrdox-config-version.cmake
    if (CMAKE_PROJECT_VERSION VERSION_LESS 1.0.0)
        set(compatibility_mode SameMajorVersion)
    else ()
        set(compatibility_mode SameMinorVersion)
    endif ()
    write_basic_package_version_file(
            mrdox-config-version.cmake
            VERSION ${PACKAGE_VERSION}
            COMPATIBILITY ${compatibility_mode})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdox-config-version.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdox)

    # Create and install mrdox-config.cmake
    set(INCLUDE_INSTALL_DIR include/)
    set(LIB_INSTALL_DIR lib/)
    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/mrdox-config.cmake.in # input file
            ${CMAKE_CURRENT_BINARY_DIR}/mrdox-config.cmake    # output file
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdox
            PATH_VARS CMAKE_INSTALL_LIBDIR INCLUDE_INSTALL_DIR LIB_INSTALL_DIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdox-config.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdox)
endif ()

#-------------------------------------------------
#
# Packages
#
#-------------------------------------------------
if (MRDOX_INSTALL AND MRDOX_PACKAGE)
    # Set the cpack variables
    # https://cliutils.gitlab.io/modern-cmake/chapters/install/packaging.html

    # The most common cpack variables
    set(CPACK_PACKAGE_VENDOR "mrdox")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.adoc")

    # Set CPACK_SOURCE_IGNORE_FILES with files source packages shouldn't install
    # We get these from .gitignore to avoid redundancy
    FILE(READ .gitignore GITIGNORE_CONTENTS)
    STRING(REGEX REPLACE ";" "\\\\;" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    STRING(REGEX REPLACE "\n" ";" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    set(CPACK_SOURCE_IGNORE_FILES ${GITIGNORE_CONTENTS})

    # Always include CPack at last
    include(CPack)
endif ()
