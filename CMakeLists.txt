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
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(
    MrDox
    VERSION 1.0.0
    DESCRIPTION "C++ Documentation Tool"
    HOMEPAGE_URL "https://github.com/cppalliance/mrdox"
    LANGUAGES CXX C
)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

#set(BUILD_SHARED_LIBS OFF CACHE STRING "")
#set(CMAKE_CXX_EXTENSIONS OFF CACHE STRING "")
# VFALCO this is to link optimized llvm for debug builds
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "")
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON CACHE STRING "")
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON CACHE STRING "")
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON CACHE STRING "")

option(MRDOX_BUILD_TESTS "Build tests" ${BUILD_TESTING})
option(MRDOX_BUILD_SHARED "Link shared" OFF)
option(MRDOX_INSTALL "Configure install target" ON)
option(MRDOX_PACKAGE "Build install package" ON)

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
    if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32") # 32-bit
        add_compile_options(
            /arch:SSE2
        )
    elseif("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win64") # 64-bit
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
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs all)
unset(CMAKE_FOLDER)

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------

configure_file(source/lib/api/Version.hpp.in ${PROJECT_BINARY_DIR}/include/mrdox/Version.hpp)

file(GLOB_RECURSE LIB_INCLUDES CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    source/lib/*.h
    source/lib/*.hpp
    source/lib/*.cpp
    source/lib/*.natvis
)

add_library(mrdox-api ${MRDOX_LINK_MODE} ${LIB_INCLUDES} ${LIB_SOURCES})
target_compile_features(mrdox-api PUBLIC cxx_std_20)
target_include_directories(mrdox-api
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/source/lib"
    ${PROJECT_BINARY_DIR}/include
)
target_compile_definitions(
    mrdox-api
    PUBLIC
    ${MRDOX_LINK_MODE_DEFINITION}
    PRIVATE
    -DMRDOX_LIB
)

# LLVM
target_link_libraries(mrdox-api PUBLIC ${llvm_libs})
target_include_directories(mrdox-api SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})

# Clang
if (CLANG_SIMPLE_LIBS)
    target_link_libraries(mrdox-api PUBLIC LLVM clang clang-cpp)
else()
    target_link_libraries(mrdox-api
        PUBLIC
        clangAST
        clangBasic
        clangFrontend
        clangIndex
        clangTooling
        clangToolingCore
        clangToolingInclusions)
endif()
target_include_directories(mrdox-api SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})

# Windows, Win64
if (WIN32)
    target_compile_definitions(
        mrdox-api
        PUBLIC
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
    )
    get_target_property(LLVM_CONFIGURATION_TYPE LLVMCore IMPORTED_CONFIGURATIONS)
    if (LLVM_CONFIGURATION_TYPE STREQUAL RELWITHDEBINFO)
        target_compile_definitions(mrdox-api PUBLIC -D_ITERATOR_DEBUG_LEVEL=0)
        target_compile_options(mrdox-api PUBLIC /MD)
    endif()
    if(MSVC)
        target_compile_options(
            mrdox-api
            PUBLIC
            /permissive-    # strict C++
            /W4             # enable all warnings
            /MP             # multi-processor compilation
            /EHsc           # C++ Exception handling
        )
    endif()
endif ()

if (MRDOX_CLANG)
    target_compile_options(
        mrdox-api
        PUBLIC
        -Wno-unused-private-field
        -Wno-unused-value
        PRIVATE
        -Wno-covered-switch-default
    )
endif ()

source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${PROJECT_SOURCE_DIR}/include/mrdox PREFIX " include" FILES ${LIB_INCLUDES})
source_group(TREE ${PROJECT_SOURCE_DIR}/source/lib/api PREFIX "api" FILES ${LIB_SOURCES})

#-------------------------------------------------
#
# Tool
#
#-------------------------------------------------

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS source/tool/*.cpp source/tool/*.hpp)
add_executable(mrdox ${TOOL_SOURCES})
target_link_libraries(mrdox PRIVATE mrdox-api)
target_include_directories(mrdox
    PRIVATE
    ${PROJECT_SOURCE_DIR}/source/lib
    ${PROJECT_BINARY_DIR}/include/
        )

source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
source_group(TREE ${PROJECT_SOURCE_DIR}/source/tool PREFIX "source" FILES ${TOOL_SOURCES})

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------

if (BUILD_TESTING)
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS source/test/*.cpp source/test/*.hpp)
    enable_testing()
    add_executable(mrdox-test ${TEST_SOURCES})
    target_link_libraries(mrdox-test PRIVATE mrdox-api ${llvm_libs})
    target_include_directories(mrdox-test
        PRIVATE
        ${PROJECT_SOURCE_DIR}/source/lib)
    add_test(NAME mrdox-test COMMAND mrdox-test
            "${PROJECT_SOURCE_DIR}/test-files/old-tests"
            )
    source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX "" FILES CMakeLists.txt)
    source_group(TREE ${PROJECT_SOURCE_DIR}/source/test PREFIX "source" FILES ${TEST_SOURCES})

    if (CMAKE_EXPORT_COMPILE_COMMANDS)
        # test run
        set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        include(mrdox.cmake)

        mrdox(FORMAT adoc CONFIG docs/mrdox.yml SOURCES ${TEST_SOURCES})
        mrdox(FORMAT xml  CONFIG docs/mrdox.yml SOURCES ${TEST_SOURCES})

        add_custom_target(reference_adoc ALL DEPENDS reference.adoc)
        add_custom_target(reference_xml  ALL DEPENDS reference.xml)
    endif()

    find_package(LibXml2)
    if (LibXml2_FOUND)
        find_package(Java REQUIRED Runtime)
        # FindJava
        if (NOT Java_FOUND)
            message(FATAL_ERROR "Java is needed to run xml-lint")
        endif()

        add_custom_target(
            mrdox.rng ALL
            DEPENDS mrdox.rnc
            COMMAND ${Java_JAVA_EXECUTABLE} -jar ${CMAKE_CURRENT_SOURCE_DIR}/tools/trang.jar
                ${CMAKE_CURRENT_SOURCE_DIR}/mrdox.rnc ${CMAKE_CURRENT_BINARY_DIR}/mrdox.rng
            SOURCES mrdox.rnc)


        file(GLOB_RECURSE XML_SOURCES CONFIGURE_DEPENDS test-files/*.xml)
        add_test(NAME xml-lint
                 COMMAND ${LIBXML2_XMLLINT_EXECUTABLE} --dropdtd --noout
                     --relaxng ${CMAKE_CURRENT_BINARY_DIR}/mrdox.rng ${XML_SOURCES}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()

#-------------------------------------------------
#
# Install
#
#-------------------------------------------------

if (MRDOX_INSTALL)
    # Create and install mrdox-targets.cmake
    install(TARGETS mrdox-api mrdox
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
