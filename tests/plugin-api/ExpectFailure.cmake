#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Run MrDocs with a plugin that has to stop the run, and pass only if the run
# failed and its output says why: checking the output alone would also pass
# a run that reported the plugin and went on. Arguments, as -D definitions:
# MRDOCS, the executable; CONFIG and OUTPUT, for its --config and --output;
# ADDONS, if set, for its --addons-supplemental; BUILTIN_ARGS, the in-tree
# directory flags with | between them, since a ; would split the definition
# that carries them; EXPECTED, a regular expression the output has to match.

if (NOT MRDOCS OR NOT CONFIG OR NOT OUTPUT OR NOT EXPECTED)
    message(FATAL_ERROR "ExpectFailure.cmake: missing required variable")
endif ()

string(REPLACE "|" ";" builtin_args "${BUILTIN_ARGS}")
set(addons_args)
if (ADDONS)
    set(addons_args "--addons-supplemental=${ADDONS}")
endif ()
execute_process(
    COMMAND "${MRDOCS}"
        "--config=${CONFIG}"
        "--output=${OUTPUT}"
        ${builtin_args}
        ${addons_args}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)

if (status EQUAL 0)
    message(FATAL_ERROR
        "MrDocs accepted the plugins it was offered:\n${output}")
elseif (NOT output MATCHES "${EXPECTED}")
    message(FATAL_ERROR
        "MrDocs failed without matching \"${EXPECTED}\":\n${output}")
endif ()
