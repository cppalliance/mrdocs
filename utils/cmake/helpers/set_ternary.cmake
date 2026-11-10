#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# set_ternary(<out_var> <condition> <if_true> <if_false>)
#
# Set <out_var> to one of two values, chosen by a condition.
#
# Example
#
#   set_ternary(MRDOCS_DO_BUNDLE_DEPENDENCIES
#       "MRDOCS_BUNDLE_DEPENDENCIES AND NOT MRDOCS_BUILD_SHARED" ON OFF)
#
# With `-D MRDOCS_BUNDLE_DEPENDENCIES=ON` and `-D MRDOCS_BUILD_SHARED=OFF` it sets
#
#   MRDOCS_DO_BUNDLE_DEPENDENCIES = ON
#
# and `OFF` when either differs. src/CMakeLists.txt then builds
# `libmrdocs-core-bundled.a` only when it is ON:
#
#   if (MRDOCS_DO_BUNDLE_DEPENDENCIES AND MRDOCS_INSTALL)
#
# CMake has no configure-time ternary, so this stands in for the recurring
# `if (cond) set(X a) else() set(X b) endif()`. It is a macro, not a function,
# so the assignment lands in the caller's scope.
#
# Parameters
#
#   <out_var>    The name of the variable to set.
#   <condition>  Anything `if()` accepts: a variable name (`MRDOCS_BUILD_SHARED`)
#                or a full expression passed as one quoted argument
#                (`"X VERSION_LESS 1.0.0"`).
#   <if_true>    The value when the condition holds. A scalar or a ;-separated
#                list.
#   <if_false>   The value when it does not. A scalar or a ;-separated list.
#
# Preconditions
#
#   It is included from the root early, before add_subdirectory(utils), because
#   the build uses it from there on.
#
# Guarantees
#
#   <out_var> is set in the calling scope, to exactly one of the two values.
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
