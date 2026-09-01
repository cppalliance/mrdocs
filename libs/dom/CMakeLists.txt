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
# (see ../README.md for the dependency rule). It never names mrdocs::Error or
# anything in mrdocs-core; errors flow upward via mrdocs::Error's implicit
# constructor from dom::Error.
file(GLOB_RECURSE DOM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(mrdocs-dom STATIC ${DOM_SOURCES})
add_library(mrdocs::dom ALIAS mrdocs-dom)

target_include_directories(mrdocs-dom
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_compile_features(mrdocs-dom PUBLIC cxx_std_23)
target_link_libraries(mrdocs-dom PUBLIC mrdocs::polyfill)

# dom owns its export macro (mrdocs/Dom/Platform.hpp -> MRDOCS_DOM_DECL); it just
# needs the link mode so that macro expands the same way (static vs shared) as
# mrdocs-core, which links this library. MRDOCS_TOOL says the library is being
# built rather than consumed, which is what marks its API for export, so that a
# plugin loaded by the tool can call it.
target_compile_definitions(mrdocs-dom PUBLIC ${MRDOCS_LINK_MODE_DEFINITION})
target_compile_definitions(mrdocs-dom PRIVATE -DMRDOCS_TOOL)

# mrdocs-core links this PUBLIC, so it must be part of the same export set.
if (MRDOCS_INSTALL)
    install(TARGETS mrdocs-dom EXPORT mrdocs-targets)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if (MRDOCS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
