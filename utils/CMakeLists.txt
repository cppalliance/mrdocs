#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Developer/maintainer tooling. Every subdirectory is wired through CMake here,
# unconditionally: its scripts are exposed as custom targets so anyone can run
# them (bootstrap, reformat, the test runner, ...) at any time, independent of
# whether tests, docs, or anything else is enabled. Only the ctest tests these
# directories register are gated on MRDOCS_BUILD_TESTS, inside each subdir.
# codegen/ is first because it also generates the project headers every build
# needs (and caches PYTHON_EXECUTABLE for the run-targets below).
add_subdirectory(codegen)
add_subdirectory(bootstrap)
add_subdirectory(danger)
add_subdirectory(docs)
add_subdirectory(linting)
add_subdirectory(testing)
add_subdirectory(cmake)
