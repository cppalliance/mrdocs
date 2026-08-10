#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Copy the addon root FROM to one whose name is not ASCII, and run
# ExpectFailure.cmake with a configuration that names the copy. The copy is
# named in a configuration file, which MrDocs reads as UTF-8, because on
# Windows its command line arrives in the ANSI code page; and it is made
# here rather than in the build, which may run its commands through that
# code page as well. Arguments, as -D definitions: FROM, the addon root to
# copy; WORK, a directory this script empties and owns; SOURCE_ROOT, the
# source root the configuration names; MRDOCS, BUILTIN_ARGS and EXPECTED, as
# for ExpectFailure.cmake.

if (NOT FROM OR NOT WORK OR NOT SOURCE_ROOT)
    message(FATAL_ERROR "NonAsciiPath.cmake: missing required variable")
endif ()

# An e with an acute accent, as its UTF-8 bytes, so that this file stays
# ASCII.
string(ASCII 195 169 e_acute)
set(addons "${WORK}/addons-${e_acute}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${addons}")
file(COPY "${FROM}/plugins" DESTINATION "${addons}")

set(CONFIG "${WORK}/mrdocs.yml")
set(OUTPUT "${WORK}/output")
file(WRITE "${CONFIG}"
    "source-root: \"${SOURCE_ROOT}\"\n"
    "file-patterns:\n"
    "  - LinkProbe.cpp\n"
    "addons-supplemental:\n"
    "  - \"${addons}\"\n")
include("${CMAKE_CURRENT_LIST_DIR}/ExpectFailure.cmake")
