#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs

# mrdocs_flags_match(<out-var> <regex>)
#
# Set <out-var> to ON when the compiler or linker flags match a regex.
#
# Example
#
#   mrdocs_flags_match(MRDOCS_SANITIZED "-fsanitize=")
#
# With `-D CMAKE_CXX_FLAGS=-fsanitize=address` it sets
#
#   MRDOCS_SANITIZED = ON
#
# and `OFF` without the flag. tests/integration/plugin-api/CMakeLists.txt then
# skips the export check, which the sanitizer runtime would fail:
#
#   if (UNIX AND NOT MRDOCS_SANITIZED)
#
# The flags are the ones of CMAKE_C_FLAGS, CMAKE_CXX_FLAGS and
# CMAKE_EXE_LINKER_FLAGS, plain and with each configuration suffix (_DEBUG,
# _RELEASE, _RELWITHDEBINFO, _MINSIZEREL, and one for each entry of
# CMAKE_CONFIGURATION_TYPES and for CMAKE_BUILD_TYPE).
#
# Parameters
#
#   <out-var>  The name of the variable that receives ON or OFF.
#   <regex>    A CMake regular expression, matched against the value of each
#              flag variable.
#
# Preconditions
#
#   The flags are final: a flag added later does not count.
#
# Guarantees
#
#   <out-var> is set to ON or OFF in the calling scope.
function(mrdocs_flags_match out_var regex)
    set(suffixes "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    foreach (config IN LISTS CMAKE_CONFIGURATION_TYPES ITEMS ${CMAKE_BUILD_TYPE})
        string(TOUPPER "${config}" config)
        if (NOT config STREQUAL "")
            list(APPEND suffixes _${config})
        endif ()
    endforeach ()
    list(REMOVE_DUPLICATES suffixes)
    foreach (suffix IN LISTS suffixes)
        foreach (variable IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS)
            if ("${${variable}${suffix}}" MATCHES "${regex}")
                set(${out_var} ON PARENT_SCOPE)
                return()
            endif ()
        endforeach ()
    endforeach ()
    set(${out_var} OFF PARENT_SCOPE)
endfunction()
