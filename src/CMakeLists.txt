#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

#-------------------------------------------------
# MrDocs header-compile build (MRDOCS_BUILD_HEADERS_ONLY)
#-------------------------------------------------
# A shortcut that compiles a single translation unit including every public
# header, producing a compile_commands.json that covers the whole API so
# mrdocs can document itself. It links nothing (no LLVM, no libs/), so it is
# defined here before the dependencies are even found, and we return so none of
# the real build runs. This is NOT the Antora docs build (MRDOCS_BUILD_DOCS).
if (MRDOCS_BUILD_HEADERS_ONLY)
    set(SRC_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
    file(GLOB_RECURSE SRC_HEADER_FILES
            "${SRC_INCLUDE_DIR}/*.hpp"
            "${PROJECT_SOURCE_DIR}/libs/dom/include/*.hpp"
            "${PROJECT_SOURCE_DIR}/libs/handlebars/include/*.hpp")
    set(BIN_INCLUDE_DIR "${PROJECT_BINARY_DIR}/include")
    file(GLOB_RECURSE BIN_HEADER_FILES "${BIN_INCLUDE_DIR}/*.hpp")
    set(ALL_HEADER_FILES ${SRC_HEADER_FILES} ${BIN_HEADER_FILES})

    set(TEMP_CPP_FILE "${PROJECT_BINARY_DIR}/all_headers.cpp")
    file(WRITE "${TEMP_CPP_FILE}" "// This file is generated automatically by CMake\n\n")
    foreach(HEADER_FILE IN LISTS ALL_HEADER_FILES)
        file(TO_CMAKE_PATH "${HEADER_FILE}" HEADER_FILE_FWD)
        file(APPEND "${TEMP_CPP_FILE}" "#include \"${HEADER_FILE_FWD}\"\n")
    endforeach()

    add_library(mrdocs-documentation-build STATIC "${TEMP_CPP_FILE}")
    target_compile_features(mrdocs-documentation-build PRIVATE cxx_std_23)
    target_include_directories(mrdocs-documentation-build
            PRIVATE
            "${SRC_INCLUDE_DIR}"
            "${BIN_INCLUDE_DIR}"
            # Public headers reference the libs/ libraries; libs/ is not added in
            # this mode, so point at their include dirs directly.
            "${PROJECT_SOURCE_DIR}/libs/polyfill/include"
            "${PROJECT_SOURCE_DIR}/libs/dom/include"
            "${PROJECT_SOURCE_DIR}/libs/handlebars/include"
    )
    target_compile_definitions(mrdocs-documentation-build PRIVATE MRDOCS_STATIC_LINK)
    return()
endif()

#-------------------------------------------------
# Dependencies (only mrdocs-core needs these)
#-------------------------------------------------
# Found here, not in the root, because the library is their only direct
# consumer. They are attached to mrdocs-core PRIVATE (see the target settings
# below): implementation details, not usage requirements, so their include
# dirs and definitions never reach any consumer. The tool and tests reach into
# MrDocs's private headers too, so they link the same mrdocs-llvm interface
# target independently, not through mrdocs-core.
set(CMAKE_FOLDER Dependencies)

# LLVM + Clang. CMP0074 makes find_package search <Pkg>_ROOT env vars, but the
# validation and Clang_ROOT derivation want the CMake variable set explicitly.
if (NOT LLVM_ROOT AND DEFINED ENV{LLVM_ROOT})
    set(LLVM_ROOT "$ENV{LLVM_ROOT}")
endif()
if (LLVM_ROOT)
    get_filename_component(LLVM_ROOT "${LLVM_ROOT}" ABSOLUTE)
    set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "Root of LLVM install." FORCE)
    if (NOT EXISTS "${LLVM_ROOT}")
        message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) provided does not exist.")
    endif()
    if (NOT EXISTS "${LLVM_ROOT}/lib/cmake/llvm")
        message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) is invalid: no <LLVM_ROOT>/lib/cmake/llvm.")
    endif()
    message(STATUS "LLVM_ROOT: ${LLVM_ROOT}")
endif()
if (Clang_ROOT)
    get_filename_component(Clang_ROOT "${Clang_ROOT}" ABSOLUTE)
    set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "Root of Clang install." FORCE)
elseif (LLVM_ROOT)
    set(Clang_ROOT "${LLVM_ROOT}" CACHE PATH "Root of Clang install." FORCE)
endif()

# Modern usage: find_package(... CONFIG) exposes imported targets. Clang gives
# per-component targets (clangTooling, clangAST, ...) that already carry the
# LLVM+Clang include dirs transitively. LLVM itself still publishes headers and
# definitions only as variables (LLVM_INCLUDE_DIRS, LLVM_DEFINITIONS): there is
# no single interface target for them in this release, so we bundle everything
# into our own mrdocs-llvm interface target below.
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

set(LIBCXX_DIR "${LLVM_INCLUDE_DIR}/c++/v1" CACHE PATH "Path to libc++ include directory")
message(STATUS "LIBCXX_DIR: ${LIBCXX_DIR}")
if (NOT EXISTS "${LIBCXX_DIR}")
    message(FATAL_ERROR "LIBCXX_DIR (${LIBCXX_DIR}) does not exist. Provide an LLVM with libc++ enabled.")
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

find_package(jerryscript REQUIRED CONFIG)
find_package(Lua CONFIG REQUIRED)

# LLVM/Clang usage requirements, bundled once as an interface target right
# after the dependency is found. LLVM publishes its headers and definitions
# only as variables (no interface target in this release), so we wrap them
# here. Both mrdocs-core and, independently, the in-tree tool and tests that
# reach into private headers pulling in LLVM link this single target.
set_ternary(MRDOCS_CLANG_LIBS CLANG_SIMPLE_LIBS
    "LLVM;clang;clang-cpp"
    "clangAST;clangBasic;clangFrontend;clangIndex;clangTooling;clangToolingCore;clangToolingInclusions")
add_library(mrdocs-llvm INTERFACE)
target_include_directories(mrdocs-llvm SYSTEM INTERFACE
        "${LLVM_INCLUDE_DIRS}"
        "${CLANG_INCLUDE_DIRS}")
target_compile_definitions(mrdocs-llvm INTERFACE ${LLVM_DEFINITIONS})
target_link_libraries(mrdocs-llvm INTERFACE ${MRDOCS_CLANG_LIBS})

unset(CMAKE_FOLDER)

#-------------------------------------------------
# mrdocs-core
#-------------------------------------------------
file(
    GLOB_RECURSE LIB_SOURCES  CONFIGURE_DEPENDS
        # Public
        ${PROJECT_SOURCE_DIR}/include/*.hpp
        ${PROJECT_SOURCE_DIR}/include/*.inc
        ${PROJECT_SOURCE_DIR}/include/*.ipp
        # Private
        ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs/*.ipp
        ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs/*.natvis
        # Natvis
        ${PROJECT_SOURCE_DIR}/include/*.natvis
)
list(APPEND LIB_SOURCES
    ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
    ${PROJECT_BINARY_DIR}/include/mrdocs/ConfigSchema.hpp
)
# These are produced at build time by the mrdocs-codegen target (utils/codegen).
# The GENERATED property set there is directory-scoped (CMP0118 is OLD at our
# cmake_minimum_required 3.13), so it is invisible here; mark them GENERATED in
# this scope or add_library rejects them as missing sources on a clean configure.
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
    ${PROJECT_BINARY_DIR}/include/mrdocs/ConfigSchema.hpp
    PROPERTIES GENERATED TRUE)
add_library(mrdocs-core ${LIB_SOURCES})
target_compile_features(mrdocs-core PUBLIC cxx_std_23)
target_include_directories(mrdocs-core
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src"
)
target_compile_definitions(
    mrdocs-core
    PUBLIC
    # Only the link mode is part of the public API (it drives MRDOCS_DECL).
    ${MRDOCS_LINK_MODE_DEFINITION}
    PRIVATE
    -DMRDOCS_TOOL
)

# Public library dependencies: the DOM, polyfills, and handlebars ARE part of
# MrDocs's public API (its public headers include <mrdocs/Dom.hpp>,
# <mrdocs/polyfill/...>, etc.), so they propagate to consumers.
target_link_libraries(mrdocs-core PUBLIC mrdocs::polyfill)
target_link_libraries(mrdocs-core PUBLIC mrdocs::dom)
target_link_libraries(mrdocs-core PUBLIC mrdocs::handlebars)

# LLVM/Clang, JerryScript, and Lua are implementation details, not part of the
# public API, so mrdocs-core depends on them PRIVATE and consumers of the
# public API never see them. LLVM/Clang come in through mrdocs-llvm, wrapped in
# $<BUILD_INTERFACE:> so the link is build-tree only: mrdocs-core is an
# installed static library, and a plain PRIVATE link would place mrdocs-llvm in
# its exported link interface ($<LINK_ONLY:>) and force it into the install
# export. The wrap keeps LLVM out of the installed interface while the wiring
# stays defined once, on mrdocs-llvm.
target_link_libraries(mrdocs-core PRIVATE $<BUILD_INTERFACE:mrdocs-llvm>)

# When mrdocs-core is installed as a static archive (the default), it still
# contains unresolved references to LLVM/Clang. A downstream target linking the
# installed mrdocs::mrdocs-core must therefore have LLVM/Clang on its own link
# line to resolve them, even though they are private (not part of the public
# API). The $<BUILD_INTERFACE:> link above is build-tree only, so it does not
# carry into the install export; re-expose the same libraries through the
# INSTALL interface. mrdocs-config.cmake.in runs find_dependency(LLVM) and
# find_dependency(Clang), so these targets are available when a consumer
# imports the package. develop linked `LLVM clang clang-cpp` PUBLIC, which had
# the same effect for installed consumers; this keeps that while still keeping
# LLVM out of the build-tree public interface. A shared mrdocs-core absorbs
# these symbols, so consumers of a shared build do not need them.
if (NOT MRDOCS_BUILD_SHARED)
    target_link_libraries(mrdocs-core INTERFACE $<INSTALL_INTERFACE:${MRDOCS_CLANG_LIBS}>)
endif ()

target_include_directories(mrdocs-core SYSTEM PRIVATE
        "${JERRYSCRIPT_INCLUDE_DIRS}"
        "${jerryscript_ROOT}/include"
        "/usr/local/include")
target_link_libraries(mrdocs-core PRIVATE jerryscript::jerry-core jerryscript::jerry-port)
target_link_libraries(mrdocs-core PRIVATE Lua::lua)

# Windows, Win64
if (WIN32)
    target_compile_definitions(
        mrdocs-core
        PUBLIC
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX20_CISO646_REMOVED_WARNING
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

# The generated ConfigSchema/Version headers come from utils/codegen; depend on
# its target so generation is ordered before this library builds.
if (TARGET mrdocs-codegen)
    add_dependencies(mrdocs-core mrdocs-codegen)
endif ()

#-------------------------------------------------
# Clang resource directory (build tree)
#-------------------------------------------------
# Replicate the clang resource directory inside our build so libclang finds it
# when running mrdocs directly from build/bin. Needs LLVM_BINARY_DIR, so it
# lives here with the LLVM dependency.
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/lib/clang")
set(RESOURCE_DIR "lib/clang/${Clang_VERSION_MAJOR}")
file(CREATE_LINK "${LLVM_BINARY_DIR}/${RESOURCE_DIR}" "${PROJECT_BINARY_DIR}/${RESOURCE_DIR}" SYMBOLIC)
if (NOT EXISTS "${LLVM_BINARY_DIR}/${RESOURCE_DIR}/include")
    message(FATAL_ERROR
            "Clang resource headers (${LLVM_BINARY_DIR}/${RESOURCE_DIR}/include) do not exist.\n"
            "Please provide an LLVM install that contains the clang resource directory.\n")
endif()

#-------------------------------------------------
# Install (the library and everything derived from its LLVM dependency)
#-------------------------------------------------
# Co-located with the target. The shared export set is finalized once, after
# the tool is also registered, by mrdocs_install() (utils/cmake/install.cmake).
if (MRDOCS_INSTALL)
    install(TARGETS mrdocs-core
            EXPORT mrdocs-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT development)

    # Public headers (source tree + generated)
    foreach(MRDOCS_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/mrdocs ${PROJECT_BINARY_DIR}/include/mrdocs)
        foreach(MRDOCS_FILE_PATTERN "*.hpp" "*.ipp" "*.inc")
            install(DIRECTORY ${MRDOCS_INCLUDE_DIR}
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                    COMPONENT development
                    FILES_MATCHING PATTERN ${MRDOCS_FILE_PATTERN})
        endforeach()
    endforeach()

    # Bundled libc++ headers and the clang resource headers (from LLVM)
    install(DIRECTORY ${LIBCXX_DIR}/
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs/headers/libcxx
            FILES_MATCHING PATTERN "*")
    install(DIRECTORY ${LLVM_BINARY_DIR}/${RESOURCE_DIR}/include
            DESTINATION ${RESOURCE_DIR}
            FILES_MATCHING PATTERN "*")
endif()
