#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# documentation-check owns a CTest documentation gate and a report target. It
# builds standalone (find_package(mrdocs)) or as part of MrDocs (the just-built
# tool). fix-loop is a docs-only example: it drives an AI agent, so it has no
# build target.
add_subdirectory(documentation-check)
