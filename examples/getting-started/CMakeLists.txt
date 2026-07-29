#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Starter projects, each invoked exactly as the user-facing docs (usage.adoc)
# instruct, to keep those instructions honest. They build standalone via their
# own run.sh, so they are registered as tests here, not added to this build.
foreach (example IN ITEMS compilation-database cmake cmake-header-only scanned)
    add_test(
        NAME mrdocs-getting-started-${example}
        COMMAND bash run.sh
                --addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons
                --output=${CMAKE_CURRENT_BINARY_DIR}/${example}/reference-output
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${example}
    )
    set_property(TEST mrdocs-getting-started-${example} PROPERTY
        ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")
endforeach ()
