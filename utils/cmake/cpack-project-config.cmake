#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# CPack reads this file once for each generator it runs, with
# `CPACK_GENERATOR` set to that generator, so it can check what the
# configure step can't know: which generators `cpack -G` asks for.

# CPack ignores `CPACK_WIX_INSTALL_SCOPE` before 3.29, without a warning,
# and builds a per-user MSI, which doesn't upgrade a per-machine install
# and changes the system `PATH` from a per-user product.
if (CPACK_GENERATOR STREQUAL "WIX" AND CMAKE_VERSION VERSION_LESS 3.29)
    message(FATAL_ERROR
            "The MSI needs CPack 3.29 or later, which honors "
            "CPACK_WIX_INSTALL_SCOPE. This is CPack ${CMAKE_VERSION}.")
endif()
