#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The cpp run logs what the page includes as expected.txt.
mrdocs_add_extension_example(count-by-kind MODE report
    FILTER "^[a-z]+: [0-9]+$" SORT
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/expected.txt")

# The generator plugin on the same theme, with a project of its own to run on.
add_subdirectory(generator)
