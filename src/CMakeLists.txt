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
# Dependencies
#-------------------------------------------------
set(CMAKE_FOLDER Dependencies)
include(setup-llvm.cmake)
find_package(jerryscript REQUIRED CONFIG)
find_package(Lua CONFIG REQUIRED)
unset(CMAKE_FOLDER)

#-------------------------------------------------
# Glob sources
#-------------------------------------------------
set(MRDOCS_GENERATED_HEADERS
    ${PROJECT_BINARY_DIR}/include/mrdocs/Version.hpp
    ${PROJECT_BINARY_DIR}/include/mrdocs/ConfigSchema.hpp)
set_source_files_properties(${MRDOCS_GENERATED_HEADERS} PROPERTIES GENERATED TRUE)
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
        # Generated
        ${MRDOCS_GENERATED_HEADERS}
)

#-------------------------------------------------
# Public headers interface target
#-------------------------------------------------
# What every target that includes the public headers needs.
add_library(mrdocs-core-headers INTERFACE)
target_include_directories(mrdocs-core-headers INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
# The polyfill is header-only, so linking it adds its include directory and the C++23 requirement.
target_link_libraries(mrdocs-core-headers INTERFACE mrdocs::polyfill)
# The core's headers include dom and handlebars headers, so a consumer needs
# their include directories, without linking the libraries themselves.
target_include_directories(mrdocs-core-headers INTERFACE
    "$<TARGET_PROPERTY:mrdocs-dom,INTERFACE_INCLUDE_DIRECTORIES>"
    "$<TARGET_PROPERTY:mrdocs-handlebars,INTERFACE_INCLUDE_DIRECTORIES>")
# Platform.hpp reads __cplusplus, which MSVC reports as 199711L without this.
target_compile_options(mrdocs-core-headers INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>")
# The variadic macros (Describe.hpp, `MRDOCS_TRY`) need the standard preprocessor.
target_compile_options(mrdocs-core-headers INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>")
# The exception model every MrDocs target uses.
target_compile_options(mrdocs-core-headers INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/EHs>")
# A class marked MRDOCS_DECL can hold a std:: member (C4251: Error has a
# std::string) or derive from a std:: type (C4275: Exception derives from
# std::exception). MSVC warns because a client with another standard library
# would see another layout. The plugin loader refuses such a plugin: it checks
# the build id and standard-library tag. So the warnings are turned off. A
# program that links mrdocs-core directly has no such check and must use the
# same compiler and standard library as the core.
target_compile_options(mrdocs-core-headers INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/wd4251;/wd4275>")
# The public headers use C++23.
# Note that we can't use any compiler version more recent than whatever is the
# latest version available on Ubuntu 20.04, since that is the oldest supported
# Linux distribution we support. Otherwise, the binary package we ship would
# not be compatible with Ubuntu 20.04, which many of our users still use.
target_compile_features(mrdocs-core-headers INTERFACE cxx_std_23)
# Windows 7 is the oldest Windows the headers support.
target_compile_definitions(mrdocs-core-headers INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0601>)
# Silences MSVC's deprecation warnings about C library functions.
target_compile_definitions(mrdocs-core-headers INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
# Silences MSVC's warning about the <ciso646> header C++20 removed.
target_compile_definitions(mrdocs-core-headers INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX20_CISO646_REMOVED_WARNING>)
# Silences clang's warning about private fields that are only read
# through reflection.
target_compile_options(mrdocs-core-headers INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-private-field>)
# Silences clang's warning about macro expansions that discard a value.
target_compile_options(mrdocs-core-headers INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-value>)
# If LLVM is built as RelWithDebInfo and MrDocs as Debug (which you typically
# should not do), this selects the release runtime and the iterator debug level
# LLVM was built with, so that the core, the tool and any plugin agree with it.
if (MSVC)
    get_target_property(LLVM_CONFIGURATION_TYPE LLVMCore IMPORTED_CONFIGURATIONS)
    if (LLVM_CONFIGURATION_TYPE STREQUAL RELWITHDEBINFO)
        target_compile_options(mrdocs-core-headers INTERFACE
            $<$<CONFIG:Debug>:/MD>
            $<$<CONFIG:Debug>:/D_ITERATOR_DEBUG_LEVEL=0>)
    endif ()
endif ()
if (MRDOCS_INSTALL)
    install(TARGETS mrdocs-core-headers EXPORT mrdocs-targets)
endif ()

#-------------------------------------------------
# Create library object files
#-------------------------------------------------
# Compiles the core's sources to object files (.o or .obj), without archiving
# or linking them. The tests link these files directly, so they can call symbols
# MRDOCS_DECL does not export. In a static build, they link every object too,
# not only the archive members they use.
add_library(mrdocs-core-object-files OBJECT ${LIB_SOURCES})
# Makes MRDOCS_DECL mean export, instead of import, while the core is built.
target_compile_definitions(mrdocs-core-object-files PRIVATE MRDOCS_SOURCE)
# The generated headers (Version.hpp, ConfigSchema.hpp) have to exist first.
add_dependencies(mrdocs-core-object-files mrdocs-codegen)
# The sources include their private headers as <mrdocs/...>, relative to src/.
target_include_directories(mrdocs-core-object-files PRIVATE "${PROJECT_SOURCE_DIR}/src")

# An executable linked with -static, -static-pie or -Wl,-static loads a plugin
# only on a system whose shared glibc is the exact version it was linked with.
mrdocs_static_link_flags(MRDOCS_HAS_STATIC_LINK_FLAGS)
# The core is compiled with MRDOCS_STATIC_LIBC when these flags are set. The
# define says the C library is linked into the executable, and a plugin that
# fails to load for this reason gets that reason added to the message of the
# loader.
target_compile_definitions(mrdocs-core-object-files PRIVATE
    $<$<BOOL:${MRDOCS_HAS_STATIC_LINK_FLAGS}>:MRDOCS_STATIC_LIBC>)

# Hidden visibility is compiled into the object files, and mrdocs-core is made
# from them, so this makes mrdocs-core export only what MRDOCS_DECL marks. The
# tests still call hidden functions: they link the object files themselves,
# and hidden only limits what a shared library exports.
# See docs/modules/ROOT/pages/contribute/invariants/binary-compatibility.adoc.
mrdocs_hide_symbols(mrdocs-core-object-files)
# CMake turns position-independent code on by default only for the target a
# SHARED library compiles, and mrdocs-core compiles nothing: these object
# files are its code, so a shared build needs them position independent.
set_ternary(mrdocs_core_pic "MRDOCS_BUILD_SHARED OR CMAKE_POSITION_INDEPENDENT_CODE" ON OFF)
set_target_properties(mrdocs-core-object-files PROPERTIES
    POSITION_INDEPENDENT_CODE "${mrdocs_core_pic}")
# Standard conformance.
target_compile_options(mrdocs-core-object-files PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->)
# The highest warning level.
target_compile_options(mrdocs-core-object-files PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
# A parallel build.
target_compile_options(mrdocs-core-object-files PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/MP>)
# Keeps the frame pointer in Debug builds, so that stack traces of failing
# tests show every frame. Only the 32-bit x86 compiler omits it; x64 and /Od
# never do.
target_compile_options(mrdocs-core-object-files PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Oy->)
# LLVM's flags (HandleLLVMOptions in setup-llvm.cmake) turn on
# -Wcovered-switch-default. The core keeps a default label in switches over
# Clang's enums, for values a newer Clang adds.
target_compile_options(mrdocs-core-object-files PRIVATE
    $<$<BOOL:${MRDOCS_CLANG}>:-Wno-covered-switch-default>)

# The include directories and the options every consumer of the headers needs.
target_link_libraries(mrdocs-core-object-files PUBLIC mrdocs-core-headers)
# Every object of dom and handlebars ends up in what links the objects, so what
# they mark for export is there whether or not the core calls it. handlebars
# links dom, so the function links dom through handlebars.
mrdocs_target_link_whole_libraries(mrdocs-core-object-files PUBLIC
    mrdocs-handlebars mrdocs-dom)
# Private dependencies
set(MRDOCS_PRIVATE_DEPENDENCIES mrdocs-llvm jerryscript::jerry-core jerryscript::jerry-port Lua::lua)
# Library that provides dlopen and dlsym for the plugin loader (empty on macOS and Windows).
list(APPEND MRDOCS_PRIVATE_DEPENDENCIES ${CMAKE_DL_LIBS})
if (NOT MRDOCS_DO_REEXPORT_DEPENDENCIES)
    # Link at build time only. It'll be bundled when installed.
    target_link_libraries(mrdocs-core-object-files PUBLIC $<BUILD_INTERFACE:${MRDOCS_PRIVATE_DEPENDENCIES}>)
else ()
    # Linked by name and propagated to consumers, who will find_dependency() them
    target_link_libraries(mrdocs-core-object-files PUBLIC ${MRDOCS_PRIVATE_DEPENDENCIES})
    install(TARGETS mrdocs-llvm EXPORT mrdocs-targets)
endif ()

#-------------------------------------------------
# Create core library target
#-------------------------------------------------
# The installed library, built from those object files:
set_ternary(mrdocs_core_type MRDOCS_BUILD_SHARED SHARED STATIC)
add_library(mrdocs-core ${mrdocs_core_type})
add_library(mrdocs::mrdocs-core ALIAS mrdocs-core)
# A consumer of the shared library links MrDocs dynamically and imports the API
# from it (on MSVC), so it has to know. Whoever links the object files or the
# static core defines nothing.
# A plugin gets the define from mrdocs::plugin-api in both modes.
target_compile_definitions(mrdocs-core PUBLIC
    $<$<BOOL:${MRDOCS_BUILD_SHARED}>:MRDOCS_DYNAMIC_LINK>)
# The objects, and through them every dependency, which is linked once there.
target_link_libraries(mrdocs-core PRIVATE mrdocs-core-object-files)
# The include directories and the options every consumer of the headers needs.
target_link_libraries(mrdocs-core PUBLIC mrdocs-core-headers)
mrdocs_target_link_whole_libraries(mrdocs-core PRIVATE mrdocs-handlebars mrdocs-dom)

# Creates the INTERFACE target mrdocs-hidden-archives. It carries the link
# options that make the binary holding the static archives behind the private
# dependencies (LLVM, Clang, Lua and JerryScript) link them without exporting
# their symbols, so only the MrDocs API is visible to plugins. Say a plugin
# bundles its own Lua and calls `lua_pushstring`: if that binary exported the
# Lua it holds, the call would bind to that copy instead of the one in the
# plugin.
mrdocs_add_hidden_archives(mrdocs-hidden-archives ${MRDOCS_PRIVATE_DEPENDENCIES})
# The binary that ends up holding the archives links them without exporting
# their symbols. The core links the target PRIVATE: a shared core holds the
# archives itself, and for a static core CMake passes the link options of its
# private dependencies on to whatever links it (policy CMP0099 is NEW), so the
# `mrdocs` tool, the reload-test host and the breaking-changes example get
# them too.
# BUILD_INTERFACE keeps the target out of the installed package, which does not
# export it.
target_link_libraries(mrdocs-core PRIVATE
    $<BUILD_INTERFACE:mrdocs-hidden-archives>)

# Args for in-tree invocations of the mrdocs
set(MRDOCS_BUILTIN_DIR_ARGS
        "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
        "--stdlib-includes=${LIBCXX_DIR}"
        "--libc-includes=${CMAKE_SOURCE_DIR}/data/mrdocs/headers/libc-stubs"
        "--clang-resource-dir=${CLANG_RESOURCE_DIR}"
        CACHE INTERNAL "Built-in directory flags for in-tree mrdocs invocations")

#-------------------------------------------------
# Install (the library and everything derived from its LLVM dependency)
#-------------------------------------------------
# Co-located with the target. The shared export set is finalized once, after
# the tool is also registered, by mrdocs_install() (utils/cmake/install.cmake).
if (MRDOCS_INSTALL)
    if (NOT MRDOCS_BUILD_SHARED)
        # A static mrdocs-core names the object library in its link interface,
        # so the export set has to carry it.
        install(TARGETS mrdocs-core-object-files EXPORT mrdocs-targets)
    endif ()
    install(TARGETS mrdocs-core
            EXPORT mrdocs-targets
            # RUNTIME is the .dll of a shared core on Windows. It goes to bin/,
            # next to mrdocs.exe, where Windows looks for it.
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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
    # Clang's builtin headers, bundled like the libcxx/libc-stubs headers. mrdocs
    # points -resource-dir here (the clang-resource-dir option) and clang appends
    # /include, so the headers install under headers/clang/include.
    install(DIRECTORY ${CLANG_RESOURCE_DIR}/include
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs/headers/clang
            FILES_MATCHING PATTERN "*")
endif()

#-------------------------------------------------
# Bundle private dependencies into mrdocs-core
#-------------------------------------------------
# Bundle mrdocs-core library with the private libraries it links
# into one self-contained fat library, so a consumer project can
# link the static mrdocs-core without the dependencies installed.
# This lets a plugin author link against the exact LLVM mrdocs
# was built with without building mrdocs from source.
if (MRDOCS_DO_BUNDLE_DEPENDENCIES AND MRDOCS_INSTALL)
    set(_mrdocs_bundled_lib_filename "${CMAKE_STATIC_LIBRARY_PREFIX}mrdocs-core-bundled${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(_mrdocs_bundled_lib_path "${CMAKE_CURRENT_BINARY_DIR}/${_mrdocs_bundled_lib_filename}")

    # Find all transitive static dependencies of mrdocs-core and the system libraries they use
    mrdocs_collect_static_libs(
            _mrdocs_bundle_deps         # The static libraries mrdocs-core references, recursively
            _mrdocs_bundle_system_libs  # The system libraries those static libraries reference, recursively
            ${MRDOCS_PRIVATE_DEPENDENCIES})

    if (WIN32)
        # Windows: lib.exe only concatenates whole .lib files.
        # There is no -r / no member-level pruning on COFF), so instead we only merge
        # only the libraries mrdocs actually *references* to make this smaller.
        # Only the referenced static libraries, so lib.exe does not embed all of LLVM.
        if (CMAKE_AR)
            set(_mrdocs_bundled_lib_path_tool "${CMAKE_AR}")
        else()
            find_program(_mrdocs_bundled_lib_path_tool NAMES lib.exe lib llvm-lib REQUIRED)
        endif()
        add_custom_command(
                OUTPUT ${_mrdocs_bundled_lib_path}
                DEPENDS mrdocs-core
                COMMAND ${CMAKE_COMMAND} -E rm -f ${_mrdocs_bundled_lib_path}
                COMMAND ${_mrdocs_bundled_lib_path_tool} /NOLOGO "/OUT:${_mrdocs_bundled_lib_path}"
                        $<TARGET_FILE:mrdocs-core> ${_mrdocs_bundle_deps}
                COMMENT "Bundling mrdocs-core with its referenced private toolchain"
                VERBATIM COMMAND_EXPAND_LISTS)
    else()
        # Unix: a relocatable link (ld -r) force-loads mrdocs-core and member-selects
        # from the collected dependency archives, so only the members mrdocs actually
        # reaches survive. The code it never calls is dropped, then re-archived. This
        # is the same list Windows merges whole; ld -r just prunes it to what is used.
        set(_mrdocs_bundled_obj_path "${CMAKE_CURRENT_BINARY_DIR}/mrdocs-core-bundled${CMAKE_C_OUTPUT_EXTENSION}")
        # mrdocs-core is force-loaded whole (it holds the roots). GNU ld resolves
        # archives in a single pass, so wrap the rest in --start-group/--end-group
        # to resolve the cross-archive references in Clang's vtables regardless of
        # order; Apple ld64 is multi-pass and rejects that flag.
        if (APPLE)
            set(_bundle_inputs
                    -Wl,-force_load,$<TARGET_FILE:mrdocs-core>
                    ${_mrdocs_bundle_deps})
        else()
            set(_bundle_inputs
                    -Wl,--whole-archive $<TARGET_FILE:mrdocs-core> -Wl,--no-whole-archive
                    -Wl,--start-group
                    ${_mrdocs_bundle_deps}
                    -Wl,--end-group)
        endif()
        add_custom_command(
                OUTPUT ${_mrdocs_bundled_lib_path}
                DEPENDS mrdocs-core
                COMMAND ${CMAKE_CXX_COMPILER} -r -nostdlib -o ${_mrdocs_bundled_obj_path} ${_bundle_inputs}
                COMMAND ${CMAKE_COMMAND} -E rm -f ${_mrdocs_bundled_lib_path}
                COMMAND ${CMAKE_AR} qcs ${_mrdocs_bundled_lib_path} ${_mrdocs_bundled_obj_path}
                COMMENT "Bundling mrdocs-core with its pruned private toolchain"
                VERBATIM COMMAND_EXPAND_LISTS)
    endif()

    add_custom_target(mrdocs-core-bundle ALL DEPENDS ${_mrdocs_bundled_lib_path})

    # Replace the thin libmrdocs-core installed above with the fat one, same path.
    # The mrdocs-targets export keeps naming it.
    install(FILES ${_mrdocs_bundled_lib_path}
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RENAME ${CMAKE_STATIC_LIBRARY_PREFIX}mrdocs-core${CMAKE_STATIC_LIBRARY_SUFFIX}
            COMPONENT development)

    # The fat archive embeds the LLVM/Clang objects but not the OS libraries they
    # reference. These are still linked to the installed target.
    if (_mrdocs_bundle_system_libs)
        target_link_libraries(mrdocs-core INTERFACE "$<INSTALL_INTERFACE:${_mrdocs_bundle_system_libs}>")
    endif ()
endif()

