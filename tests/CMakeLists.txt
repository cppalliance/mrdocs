#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Each test suite owns its own CMakeLists.
# Public-API unit tests (mrdocs-unit-tests).
add_subdirectory(unit)
# Reference-output harness (mrdocs-golden-tests).
add_subdirectory(golden)
# Benchmarks are custom targets, not CTest tests; see benchmarks/README.md.
add_subdirectory(benchmarks)
# Suites that run the built tool or the installed package from outside, as a
# user does; see integration/README.md.
add_subdirectory(integration)
# mrdocs run over its own sources with project checks as addons.
add_subdirectory(self-doc)
