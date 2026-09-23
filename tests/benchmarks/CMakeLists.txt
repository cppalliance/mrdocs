#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Benchmarks are not tests: they are timing-sensitive and slow, so none
# is registered with CTest. Each is a custom target you run on demand.

find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
    # Script engine comparison: JavaScript versus Lua over MrDocs's own
    # corpus. Prints a median table; --asciidoc gives the table the
    # docs page (extensions/script-engines.adoc) reproduces.
    add_custom_target(mrdocs-benchmark-script-engines
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/script-engines/run.py"
            "--mrdocs=$<TARGET_FILE:mrdocs>"
            "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
            "--runs=30"
        DEPENDS mrdocs
        USES_TERMINAL
        COMMENT "Script engine benchmarks (JavaScript vs Lua)"
    )
endif()
