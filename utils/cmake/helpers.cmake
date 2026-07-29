#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# set_ternary(<out_var> <condition> <if_true> <if_false>)
#
# CMake has no configure-time ternary, so this stands in for the recurring
# `if (cond) set(X a) else() set(X b) endif()`. It is a macro (not a function)
# so the assignment lands in the caller's scope. <condition> is evaluated by
# if(), so it may be a variable name (`MRDOCS_BUILD_SHARED`) or a full
# expression passed as one quoted argument (`"X VERSION_LESS 1.0.0"`). Values
# may be scalars or ; -separated lists.
macro(set_ternary out_var cond val_true val_false)
    # <cond> is substituted into the source text and re-parsed, so a compound
    # expression (`X MATCHES "Y"`, `X VERSION_LESS 1.0.0`) evaluates the same as
    # if it were written inline. A plain `if (${cond})` would instead expand the
    # string into already-tokenized arguments, which mis-evaluates operators
    # like MATCHES/STREQUAL and silently takes the false branch.
    cmake_language(EVAL CODE "
        if (${cond})
            set(${out_var} \"${val_true}\")
        else ()
            set(${out_var} \"${val_false}\")
        endif ()
    ")
endmacro()