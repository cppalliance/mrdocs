#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# handlebars: the Handlebars template engine, extracted from mrdocs/Engines. A
# compiled STATIC library that depends ONLY on the standard library, the
# polyfills library, and dom (see ../README.md for the dependency rule). It
# never names mrdocs::Error; errors flow upward via mrdocs::Error's implicit
# constructor from handlebars::Error (which in turn converts from dom::Error).
file(GLOB_RECURSE HANDLEBARS_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(mrdocs-handlebars STATIC ${HANDLEBARS_SOURCES})
add_library(mrdocs::handlebars ALIAS mrdocs-handlebars)

target_include_directories(mrdocs-handlebars
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_compile_features(mrdocs-handlebars PUBLIC cxx_std_23)
target_link_libraries(mrdocs-handlebars PUBLIC mrdocs::dom mrdocs::polyfill)

# handlebars owns its export macro (mrdocs/Engines/Handlebars/Platform.hpp ->
# MRDOCS_HANDLEBARS_DECL); it just needs the link mode so that macro expands the
# same way (static vs shared) as mrdocs-core, which links this library.
target_compile_definitions(mrdocs-handlebars PUBLIC ${MRDOCS_LINK_MODE_DEFINITION})
target_compile_definitions(mrdocs-handlebars PRIVATE -DMRDOCS_TOOL)

# mrdocs-core links this PUBLIC, so it must be part of the same export set.
if (MRDOCS_INSTALL)
    install(TARGETS mrdocs-handlebars EXPORT mrdocs-targets)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if (MRDOCS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
