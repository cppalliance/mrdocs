#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Dependency-handling fixtures, each exercised with the same mrdocs invocation
# the docs use.
foreach (example IN ITEMS find shim-files shim-snippets accept-missing)
    add_test(
        NAME mrdocs-dependencies-${example}
        COMMAND mrdocs docs/mrdocs.yml
                --output ${CMAKE_CURRENT_BINARY_DIR}/${example}/reference-output
                ${MRDOCS_BUILTIN_DIR_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${example}
    )
endforeach ()
