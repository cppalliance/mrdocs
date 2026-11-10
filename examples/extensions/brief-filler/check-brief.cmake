#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Fail unless the XML output holds the brief that the plugin's transform gives
# the symbols nobody documented. Run with -DXML=<file>.
set(BRIEF "Undocumented.")
file(READ "${XML}" content)
string(FIND "${content}" "<literal>${BRIEF}</literal>" position)
if (position EQUAL -1)
    message(FATAL_ERROR "the brief \"${BRIEF}\" is missing from ${XML}")
endif ()
