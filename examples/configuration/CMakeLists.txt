#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Per-option configuration examples from the docs. Each lives at
# configuration/<page>/<example>/ with a docs/mrdocs.yml and the minimum
# source the demonstrated option needs. Each is exercised as a test, which
# checks that the run succeeds: none of them commits its output.
set(_config_examples
    "inputs/extra-includes"
    "parsing/missing-include-prefixes"
    "parsing/missing-include-shims"
    "diagnostics/lint"
    "diagnostics/logging"
    "diagnostics/runtime"
    "overview/customized"
)
foreach (example IN LISTS _config_examples)
    mrdocs_add_example(DIRECTORY ${example} CONFIG docs/mrdocs.yml RUN_ONLY)
endforeach ()
