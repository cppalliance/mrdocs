#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_static_link_flags(<out-var>)
#
# Set <out-var> to ON when the flags ask for a static link.
#
# Example
#
#   mrdocs_static_link_flags(MRDOCS_HAS_STATIC_LINK_FLAGS)
#
# With `-D CMAKE_EXE_LINKER_FLAGS=-static` it sets
#
#   MRDOCS_HAS_STATIC_LINK_FLAGS = ON
#
# and `OFF` with no flag, or with `-staticx`. src/CMakeLists.txt then gives the
# core a definition. release-linux.cmake makes its own call, with
# MRDOCS_STATIC_LINK_FLAGS, and stops the configure on ON:
#
#   target_compile_definitions(mrdocs-core-object-files PRIVATE
#       $<$<BOOL:${MRDOCS_HAS_STATIC_LINK_FLAGS}>:MRDOCS_STATIC_LIBC>)
#
# The flags are the ones mrdocs_flags_match reads. A static link is `-static`,
# `--static`, `-static-pie` or `-Wl,-static`, as a whole word of the flags.
#
# Parameters
#
#   <out-var>  The name of the variable that receives ON or OFF.
#
# Preconditions
#
#   The flags are final: a flag added later does not count.
#
# Guarantees
#
#   <out-var> is set to ON or OFF in the calling scope.
function(mrdocs_static_link_flags out_var)
    mrdocs_flags_match(is_static
        "(^|[ \t\r\n;])(--?static(-pie)?|-Wl,-static)($|[ \t\r\n;])")
    set(${out_var} ${is_static} PARENT_SCOPE)
endfunction()
