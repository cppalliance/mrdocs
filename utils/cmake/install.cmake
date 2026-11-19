#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
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
# Finalize installation and packaging: the single `mrdocs-targets` export, the
# package config files, and CPack when packages are created.
#
# Example
#
#   mrdocs_install()
#
# leaves `lib/cmake/mrdocs/mrdocs-config.cmake` in the install tree, so that
# `find_package(mrdocs)` defines `mrdocs::mrdocs-core`, `mrdocs::plugin-api`
# and `mrdocs::mrdocs`.
#
# Parameters
#
#   None.
#
# Preconditions
#
#   Call it once, from the root, after src/ and tools/ registered their
#   targets in `mrdocs-targets`: the individual targets install themselves next
#   to their definitions, and the export spans them.
#
# Guarantees
#
#   It installs the export, the package config and version files, and the
#   helpers a consumer includes.
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
    #
    # The calendar release label (the release tag on HEAD, or the UTC commit
    # date) and the plugin build id (the commit, with ".modified" for a tree
    # with edits, or MRDOCS_BUILD_ID) come from git, read at configure time by
    # the same script that stamps Version.hpp so both agree. PYTHON_EXECUTABLE
    # and GIT_EXECUTABLE are cache entries set by utils/codegen.
    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/utils/codegen/generate-version-header.py
                    --print-release
                    --source-dir ${PROJECT_SOURCE_DIR}
                    --git "${GIT_EXECUTABLE}"
                    --build-id "${MRDOCS_BUILD_ID}"
            OUTPUT_VARIABLE MRDOCS_RELEASE_FIELDS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)
    list(GET MRDOCS_RELEASE_FIELDS 0 MRDOCS_RELEASE)
    list(GET MRDOCS_RELEASE_FIELDS 1 MRDOCS_RELEASE_WITH_BUILD)
    list(GET MRDOCS_RELEASE_FIELDS 2 MRDOCS_RELEASE_YEAR)
    list(GET MRDOCS_RELEASE_FIELDS 3 MRDOCS_RELEASE_MONTH)
    list(GET MRDOCS_RELEASE_FIELDS 4 MRDOCS_RELEASE_DAY)
    list(GET MRDOCS_RELEASE_FIELDS 5 MRDOCS_BUILD_ID_VALUE)

    set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
    set(BIN_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
    set(DATAROOT_INSTALL_DIR ${CMAKE_INSTALL_DATAROOTDIR})
    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs-config.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs
            PATH_VARS CMAKE_INSTALL_LIBDIR INCLUDE_INSTALL_DIR LIB_INSTALL_DIR BIN_INSTALL_DIR DATAROOT_INSTALL_DIR)
    # The helpers a consumer includes from mrdocs-config.cmake.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mrdocs-config.cmake
                  ${PROJECT_SOURCE_DIR}/tools/cmake/mrdocs-helpers.cmake
                  ${PROJECT_SOURCE_DIR}/tools/cmake/mrdocs_ensure_runtime_dlls.cmake
                  ${PROJECT_SOURCE_DIR}/tools/cmake/mrdocs_add_plugin.cmake
                  ${PROJECT_SOURCE_DIR}/tools/cmake/LinkOrCopyDlls.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mrdocs)

    # The Antora site, if it was built (docs/ produces it under docs/build/site).
    set(DOCS_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/build/site)
    if (MRDOCS_BUILD_DOCS AND EXISTS ${DOCS_BUILD_DIR})
        install(DIRECTORY ${DOCS_BUILD_DIR}
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
                COMPONENT documentation)
    endif()

    # CPack packaging, when MRDOCS_PACKAGE is on.
    if (MRDOCS_PACKAGE)
        mrdocs_package()
    endif()
endfunction()

# mrdocs_package()
#
# Configure CPack for the packages (`cpack`) and the source archive.
#
# Example
#
#   cmake -D MRDOCS_PACKAGE=ON ..
#   cpack
#
# Parameters
#
#   None. It reads the project version and description, `LICENSE.txt`,
#   `README.adoc` and `.gitignore`.
#
# Preconditions
#
#   Called by mrdocs_install when MRDOCS_PACKAGE is on, after the install rules
#   exist.
#
# Guarantees
#
#   The version, license and readme of the packages come from the project, and
#   the source archive leaves out what `.gitignore` lists. It includes CPack,
#   which defines the `package` targets.
function(mrdocs_package)
    set(CPACK_PACKAGE_VENDOR "mrdocs")
    set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Library and headers")
    set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION
            "The mrdocs-core static library and the MrDocs headers, for building against MrDocs.")
    set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.adoc")

    # The MSI's upgrade code identifies MrDocs across releases, so an MSI
    # replaces the MrDocs already installed. Never change it. CPack's
    # template allows upgrades between equal versions, so an older MSI
    # with the same project version replaces a newer install.
    set(CPACK_WIX_UPGRADE_GUID "CBB0618D-E748-4848-B57A-167981E3C241")

    # The MSI can add mrdocs to the system `PATH`, which only a
    # per-machine install may change.
    set(CPACK_WIX_INSTALL_SCOPE perMachine)
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/utils/cmake/wix-patch.xml.in
            ${CMAKE_CURRENT_BINARY_DIR}/wix-patch.xml
            @ONLY)
    set(CPACK_WIX_PATCH_FILE ${CMAKE_CURRENT_BINARY_DIR}/wix-patch.xml)
    # The patch extends this UI, which CPack would otherwise choose only
    # because some install rules name a component.
    set(CPACK_WIX_UI_REF WixUI_FeatureTree)
    set(CPACK_PROJECT_CONFIG_FILE
            ${CMAKE_CURRENT_SOURCE_DIR}/utils/cmake/cpack-project-config.cmake)

    # Ignore files (from .gitignore)
    FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore GITIGNORE_CONTENTS)
    STRING(REGEX REPLACE ";" "\\\\;" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    STRING(REGEX REPLACE "\n" ";" GITIGNORE_CONTENTS "${GITIGNORE_CONTENTS}")
    set(CPACK_SOURCE_IGNORE_FILES ${GITIGNORE_CONTENTS})

    include(CPack)
endfunction()
