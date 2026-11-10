#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The suites in this directory use the built tool from outside, as a user does,
# and every test of them carries the `integration` label:
#   ctest --test-dir <build> -L integration
#
# cmake/ is a separate project that CI configures against an install
# prefix, so no add_subdirectory reaches it.
add_subdirectory(plugin-api)
