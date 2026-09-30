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
# instruct (`mrdocs --config=docs/mrdocs.yml` from the project's root), to keep
# those instructions honest. They are standalone projects, so they are
# registered as examples here, not added to this build, and commit no output.
foreach (example IN ITEMS compilation-database cmake cmake-header-only scanned)
    mrdocs_add_example(DIRECTORY ${example} CONFIG docs/mrdocs.yml RUN_ONLY)
endforeach ()
