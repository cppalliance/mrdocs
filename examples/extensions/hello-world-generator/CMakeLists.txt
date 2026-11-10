#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

mrdocs_add_extension_example(hello-world-generator MODE file
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}"
    GENERATOR hello-world-generator OUTPUT_FILE hello.txt
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/expected.txt")
