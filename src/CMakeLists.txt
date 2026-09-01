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
# Create target
#-------------------------------------------------
add_library(mrdocs-core ${LIB_SOURCES})
add_library(mrdocs::mrdocs-core ALIAS mrdocs-core)
add_dependencies(mrdocs-core mrdocs-codegen)
target_compile_features(mrdocs-core PUBLIC cxx_std_23)
target_include_directories(mrdocs-core
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(
    mrdocs-core
    PUBLIC
    ${MRDOCS_LINK_MODE_DEFINITION}
    $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0601>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX20_CISO646_REMOVED_WARNING>
    PRIVATE
    -DMRDOCS_TOOL
)
target_compile_options(
    mrdocs-core
    PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>  # Platform.hpp
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor> # Describe.hpp uses __VA_OPT__
    $<$<BOOL:${MRDOCS_CLANG}>:-Wno-unused-private-field>
    $<$<BOOL:${MRDOCS_CLANG}>:-Wno-unused-value>
    PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive- /W4 /MP>
    # C4251 and C4275: MRDOCS_DECL now marks the exported types, whose standard
    # library members and bases carry no export marker of their own. LLVM's own
    # copies of these suppressions are stripped from the global flags in
    # setup-llvm.cmake, so name them where the exporting happens.
    $<$<CXX_COMPILER_ID:MSVC>:/wd4251 /wd4275>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<STREQUAL:${CMAKE_GENERATOR_PLATFORM},Win64>>:/arch:AMD64>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Oy->  # Disable frame pointer omission
    $<$<BOOL:${MRDOCS_CLANG}>:-Wno-covered-switch-default>
)

# Args for in-tree invocations of the mrdocs
set(MRDOCS_BUILTIN_DIR_ARGS
        "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
        "--stdlib-includes=${LIBCXX_DIR}"
        "--libc-includes=${CMAKE_SOURCE_DIR}/data/mrdocs/headers/libc-stubs"
        "--clang-resource-dir=${CLANG_RESOURCE_DIR}"
        CACHE INTERNAL "Built-in directory flags for in-tree mrdocs invocations")

#-------------------------------------------------
# Dependency linking
#-------------------------------------------------
# Public dependencies: linked by name and propagated to consumers
target_link_libraries(mrdocs-core PUBLIC mrdocs::polyfill mrdocs::dom mrdocs::handlebars)

# Private dependencies
set(MRDOCS_PRIVATE_DEPENDENCIES mrdocs-llvm jerryscript::jerry-core jerryscript::jerry-port Lua::lua)
if (NOT MRDOCS_DO_REEXPORT_DEPENDENCIES)
    # Link at build time only. It’ll be bundled when installed.
    target_link_libraries(mrdocs-core PRIVATE $<BUILD_INTERFACE:${MRDOCS_PRIVATE_DEPENDENCIES}>)
else ()
    # Linked by name and propagated to consumers, who will find_dependency() them
    target_link_libraries(mrdocs-core PRIVATE ${MRDOCS_PRIVATE_DEPENDENCIES})
    install(TARGETS mrdocs-llvm EXPORT mrdocs-targets)
endif ()

# You typically should not be doing it, but if someone attempts to link LLVM in MSVC/Release
# with MrDocs in MSVC/Debug, adjust flags to avoid a mismatch and propagate them to consumers.
if(MSVC)
    get_target_property(LLVM_CONFIGURATION_TYPE LLVMCore IMPORTED_CONFIGURATIONS)
    if (LLVM_CONFIGURATION_TYPE STREQUAL RELWITHDEBINFO)
        # Handle Debug/RelWithDebInfo mismatch between mrdocs and LLVM
        target_compile_definitions(mrdocs-core PUBLIC $<$<CONFIG:Debug>:-D_ITERATOR_DEBUG_LEVEL=0>)
        target_compile_options(mrdocs-core PUBLIC $<$<CONFIG:Debug>:/MD>)
    endif()
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

