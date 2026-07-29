#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# polyfills: stand-ins for standard-library features not yet available on every
# supported compiler (see ../README.md). All header-only today, so this is an
# INTERFACE library. Depends ONLY on the standard library. Deleted piece by
# piece as each feature becomes universally available.
add_library(mrdocs-polyfill INTERFACE)
add_library(mrdocs::polyfill ALIAS mrdocs-polyfill)
target_include_directories(mrdocs-polyfill
        INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(mrdocs-polyfill INTERFACE cxx_std_23)

# mrdocs-core links this PUBLIC, so it must be part of the same export set.
if (MRDOCS_INSTALL)
    install(TARGETS mrdocs-polyfill EXPORT mrdocs-targets)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if (MRDOCS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
