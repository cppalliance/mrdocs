#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# handlebars is tested independently of mrdocs-core. The tests are split the way
# the top-level mrdocs tests are: `unit` holds in-code behavior checks (no LLVM,
# no fixtures), `golden` holds reference/spec tests that read fixtures and parse
# JSON with LLVM. Shared test helpers live in HandlebarsTestSupport.hpp.
add_subdirectory(unit)
add_subdirectory(golden)
