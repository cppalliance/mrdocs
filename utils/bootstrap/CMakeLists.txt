#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The bootstrap dependency/build setup tool. Exposed as custom targets so it
# can be run at any time, not tied to tests.
add_custom_target(bootstrap
    COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/bootstrap.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Run the bootstrap setup (bootstrap.py)")
add_custom_target(refresh-ide-configs
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/refresh_ide_configs.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Regenerate the IDE run configurations")

# Python unit tests for the bootstrap tooling (CI "Bootstrap Python Tests").
# Not registered on Windows yet. The test-module imports were fixed to load on
# Windows (they no longer split __file__ on a hardcoded "/"), but several
# assertions still assume POSIX paths (they compare os.path.join results, which
# use "\" on Windows, against hardcoded "/" literals), so the suite is not
# Windows-clean at the assertion level. The tooling is exercised by the
# dedicated (Linux) CI job, so gating here only skips the local ctest
# convenience wiring on Windows. Drop the NOT WIN32 once the assertions are made
# separator-agnostic and verified on a Windows runner.
if (MRDOCS_BUILD_TESTS AND NOT WIN32)
    add_test(NAME utils-bootstrap-tests
        COMMAND ${PYTHON_EXECUTABLE} -m unittest discover -s utils/bootstrap/tests
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif ()
