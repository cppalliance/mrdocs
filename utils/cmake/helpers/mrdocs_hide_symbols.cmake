#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_hide_symbols(<target>)
#
# Compile a target with hidden visibility, so that what it exports is what
# MRDOCS_DECL (or the library's own marker) carries in the headers.
#
# Example
#
#   mrdocs_hide_symbols(mrdocs-core-object-files)
#
# sets on the target
#
#   CXX_VISIBILITY_PRESET     = hidden
#   VISIBILITY_INLINES_HIDDEN = ON
#
# so its compile commands in `compile_commands.json` carry
# `-fvisibility=hidden -fvisibility-inlines-hidden` on Linux and macOS, and
# only what MRDOCS_DECL marks is exported.
#
# Parameters
#
#   <target>  The library or executable to compile.
#
# Preconditions
#
#   <target> exists. Call it before the target's sources are compiled, which
#   means any time in the configure step.
#
# Guarantees
#
#   <target> gets CXX_VISIBILITY_PRESET and VISIBILITY_INLINES_HIDDEN, so no
#   symbol is exported unless it is marked. MSVC exports only what is marked
#   anyway.
function(mrdocs_hide_symbols target)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endfunction()
