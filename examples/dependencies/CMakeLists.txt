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
# the docs use, the configuration given as a plain argument. None commits its
# output.
foreach (example IN ITEMS find shim-files shim-snippets accept-missing)
    mrdocs_add_example(DIRECTORY ${example} ARGS docs/mrdocs.yml RUN_ONLY)
endforeach ()
