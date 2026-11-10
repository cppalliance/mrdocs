#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# dom: the document object model, extracted from mrdocs/Dom. A compiled STATIC
# library that depends ONLY on the standard library and the polyfills library
# (see docs/modules/ROOT/pages/contribute/layout/libs.adoc for the dependency
# rule). It never names mrdocs::Error or anything in mrdocs-core; errors flow
# upward via mrdocs::Error's implicit constructor from dom::Error.
file(GLOB_RECURSE DOM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(mrdocs-dom STATIC ${DOM_SOURCES})
add_library(mrdocs::dom ALIAS mrdocs-dom)

# A shared core links this archive whole, so its objects have to be position
# independent.
if (MRDOCS_BUILD_SHARED)
    set_target_properties(mrdocs-dom PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()

target_include_directories(mrdocs-dom
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_compile_features(mrdocs-dom PUBLIC cxx_std_23)
target_link_libraries(mrdocs-dom PUBLIC mrdocs::polyfill)

# dom owns its export macro (mrdocs/Dom/Platform.hpp -> MRDOCS_DOM_DECL).
# Marks the API for export. Set on this target alone, while it is built from
# source.
target_compile_definitions(mrdocs-dom PRIVATE MRDOCS_DOM_SOURCE)

# What carries MRDOCS_DOM_DECL is the exported API.
mrdocs_hide_symbols(mrdocs-dom)

# mrdocs-core-object-files links this PUBLIC, and a static mrdocs-core names
# that target in its interface, so it must be part of the same export set.
if (MRDOCS_INSTALL)
    install(TARGETS mrdocs-dom EXPORT mrdocs-targets)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if (MRDOCS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
