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
# configuration/<page>/<example>/ with a docs/mrdocs.yml, a run.sh, and the
# minimum source the demonstrated option needs. Each is exercised as a test.
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
    string(REPLACE "/" "-" _name ${example})
    add_test(
        NAME mrdocs-configuration-${_name}
        COMMAND bash run.sh
                --addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons
                --output=${CMAKE_CURRENT_BINARY_DIR}/${_name}/reference-output
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${example}
    )
    set_property(TEST mrdocs-configuration-${_name} PROPERTY
        ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")
endforeach ()
