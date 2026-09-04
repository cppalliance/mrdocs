#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Build-support CMake module.
#
# It does nothing on include: it only defines the mrdocs_install function
# the project can call.
#
include_guard(GLOBAL)

# mrdocs_install()
#
# Finalizes installation and packaging.
#
# The individual targets install themselves next to their definitions
# (mrdocs-core in src/, mrdocs in tools/, shared data in data/).
#
# This finalizes the single mrdocs-targets export (which spans src/ and tools/,
# so it must be done once, after both are registered).
#
# It writes the package config files, and configures CPack if packages are created.
function(mrdocs_install)
    install(EXPORT mrdocs-targets
            FILE mrdocs-targets.cmake
            NAMESPACE mrdocs::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    set(CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    # mrdocs-config-version.cmake
    set_ternary(compatibility_mode "CMAKE_PROJECT_VERSION VERSION_LESS 1.0.0"
                SameMajorVersion SameMinorVersion)
    write_basic_package_version_file(
            mrdocs-config-version.cmake
            VERSION ${PACKAGE_VERSION}
            COMPATIBILITY ${compatibility_mode})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config-version.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    # mrdocs-config.cmake
    set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
    set(BIN_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
    set(DATAROOT_INSTALL_DIR ${CMAKE_INSTALL_DATAROOTDIR})
    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs-config.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs
            PATH_VARS CMAKE_INSTALL_LIBDIR INCLUDE_INSTALL_DIR LIB_INSTALL_DIR BIN_INSTALL_DIR DATAROOT_INSTALL_DIR)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    # The Antora site, if it was built (docs/ produces it under docs/build/site).
    set(DOCS_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/build/site)
    if (MRDOCS_BUILD_DOCS AND EXISTS ${DOCS_BUILD_DIR})
        install(DIRECTORY ${DOCS_BUILD_DIR}
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
                COMPONENT documentation)
    endif()

    # CPack packaging.
    if (MRDOCS_PACKAGE)
        mrdocs_package()
    endif()
endfunction()

function(mrdocs_package)
    set(CPACK_PACKAGE_VENDOR "mrdocs")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.adoc")

    # Ignore files (from .gitignore)
    FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore GITIGNORE_CONTENTS)
    STRING(REGEX REPLACE ";" "\\\\;" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    STRING(REGEX REPLACE "\n" ";" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    set(CPACK_SOURCE_IGNORE_FILES ${GITIGNORE_CONTENTS})

    include(CPack)
endfunction()
