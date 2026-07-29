#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# This directory only installs assets, so it does nothing unless installing.
if (MRDOCS_BUILD_HEADERS_ONLY OR NOT MRDOCS_INSTALL)
    return()
endif()

# Install the shared assets this directory owns into the FHS share/ location.
# (The LLVM-derived assets, bundled libc++ headers and the clang resource dir,
# are installed by src/, next to the dependency that provides them.)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs/headers/libc-stubs/
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs/headers/libc-stubs
        FILES_MATCHING PATTERN "*")

foreach (mrdocs_dir addons)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mrdocs/${mrdocs_dir}
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs
            FILES_MATCHING PATTERN "*")
endforeach ()
foreach (data_dir cmake gdb)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${data_dir}
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mrdocs
            FILES_MATCHING PATTERN "*")
endforeach ()
