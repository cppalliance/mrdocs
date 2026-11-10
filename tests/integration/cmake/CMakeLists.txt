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

cmake_minimum_required(VERSION 3.21)
project(mrdocs_cmake_consumer LANGUAGES CXX C)

# Discovery is driven by the exact-case mrdocs_ROOT (honored since CMake 3.12),
# so no policy opt-in is needed.
find_package(mrdocs REQUIRED)
enable_testing()

# One subdirectory per way of using the package. Each holds its own sources and
# tests, and every test of the project belongs to the integration category.
add_subdirectory(executable)
add_subdirectory(plugin)
add_subdirectory(library)
