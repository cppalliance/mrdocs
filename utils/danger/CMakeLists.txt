#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Danger.js pull-request checks. The Danger.js unit tests are run
# authoritatively by the dedicated "Utility Tests" CI job, which installs the
# node dependencies and runs `npx vitest run`. We also register them under
# ctest as a local convenience, but only when the dependencies are already
# installed (utils/danger/node_modules present), and we run vitest directly
# rather than doing a network `npm ci` at test time.
#
# An earlier version registered the test unconditionally and let it `npm ci` on
# demand. That made every C++ build/test job depend on network access and node
# tooling, and it flaked for reasons unrelated to the C++ build: CMake could not
# spawn npm on Windows, and `npm ci` hit transient ECONNRESET on Linux. When
# node_modules is absent the test is simply not registered; that is not a false
# all-green, because the Utility Tests job covers Danger.js on every push.
find_program(NPX_EXECUTABLE npx)
if (MRDOCS_BUILD_TESTS)
    if (NPX_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/node_modules")
        add_test(NAME utils-danger-tests
            COMMAND ${NPX_EXECUTABLE} vitest run
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    else ()
        message(STATUS
            "utils-danger-tests not registered under ctest (no "
            "utils/danger/node_modules). Run `npm ci` in utils/danger to enable "
            "it locally; CI covers Danger.js via the Utility Tests job.")
    endif ()
endif ()
