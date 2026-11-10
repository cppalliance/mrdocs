#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The refusals of a plugin that the Defining Extensions page lists. Each of
# the plugins above has an addon root of its own.
set(MRDOCS_REFUSAL_MESSAGES
    foreign-build-id foreign-tag null-build-id null-tag not-a-plugin
    main-error main-false)
if (NOT WIN32)
    list(APPEND MRDOCS_REFUSAL_MESSAGES undefined-symbol)
endif ()
foreach (refusal IN LISTS MRDOCS_REFUSAL_MESSAGES)
    # A run on that root ends with the message in <name>.txt next to this
    # file, which is what the test expects. The page shows the same text
    # inline. To rewrite a file after the message changed on purpose, run the
    # test command with -DUPDATE=ON.
    add_test(NAME mrdocs-plugin-${refusal}-message
        COMMAND ${CMAKE_COMMAND}
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/refused/messages/${refusal}.txt"
            "-DRUN_COMMAND=${PROJECT_SOURCE_DIR}/utils/cmake/helpers/RunCommand.cmake"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/refused/messages/CompareMessage.cmake"
            --
            $<TARGET_FILE:mrdocs>
            "--config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml"
            "--output=${CMAKE_CURRENT_BINARY_DIR}/${refusal}-message-output"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            "--addons-supplemental=${CMAKE_CURRENT_BINARY_DIR}/${refusal}")
endforeach ()

# A transform that raises an error stops the run, and the message names it.
add_test(NAME mrdocs-transform-failure-message
    COMMAND ${CMAKE_COMMAND}
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/refused/messages/failing-transform.txt"
        "-DRUN_COMMAND=${PROJECT_SOURCE_DIR}/utils/cmake/helpers/RunCommand.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/refused/messages/CompareMessage.cmake"
        --
        $<TARGET_FILE:mrdocs>
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/project/mrdocs.yml"
        --generator=noop
        "--output=${CMAKE_CURRENT_BINARY_DIR}/failing-transform-message-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${CMAKE_CURRENT_SOURCE_DIR}/refused/failing-transform")
