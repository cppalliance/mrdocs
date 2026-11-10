#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# A -P script: runs the installed LinkOrCopyDlls.cmake on made-up DLL files and
# checks which ones it puts next to the target. The inputs are SCRIPT (the
# installed script) and WORK (a scratch directory).
#
# The files, all under WORK:
#   src/same.dll    also in path/ with the same content: skipped
#   src/other.dll   also in path/ with other content: linked
#   src/local.dll   also in out/ with the same content: skipped
#   src/fresh.dll   nowhere else: linked
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/src" "${WORK}/path" "${WORK}/path2" "${WORK}/out")
file(WRITE "${WORK}/src/same.dll" "same")
file(WRITE "${WORK}/path/same.dll" "same")
file(WRITE "${WORK}/src/other.dll" "mine")
file(WRITE "${WORK}/path/other.dll" "theirs")
file(WRITE "${WORK}/src/local.dll" "local")
file(WRITE "${WORK}/out/local.dll" "local")
file(WRITE "${WORK}/src/fresh.dll" "fresh")

if (WIN32)
    set(separator ";")
else ()
    set(separator ":")
endif ()
set(dlls "${WORK}/src/same.dll;${WORK}/src/other.dll;${WORK}/src/local.dll;${WORK}/src/fresh.dll")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${WORK}/path2${separator}${separator}${WORK}/path"
        ${CMAKE_COMMAND}
            "-DDLLS=${dlls}"
            "-DDIR=${WORK}/out"
            -P "${SCRIPT}"
    RESULT_VARIABLE result)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "LinkOrCopyDlls.cmake failed: ${result}")
endif ()

foreach (name IN ITEMS other fresh)
    if (NOT EXISTS "${WORK}/out/${name}.dll")
        message(FATAL_ERROR "${name}.dll was not put next to the target")
    endif ()
endforeach ()
if (IS_SYMLINK "${WORK}/out/local.dll")
    message(FATAL_ERROR "local.dll was replaced by a link, but the same file was already there")
endif ()
if (EXISTS "${WORK}/out/same.dll")
    message(FATAL_ERROR "same.dll was put next to the target, but the same file is on PATH")
endif ()
file(SHA256 "${WORK}/out/other.dll" other_hash)
file(SHA256 "${WORK}/src/other.dll" other_expected)
if (NOT other_hash STREQUAL other_expected)
    message(FATAL_ERROR "other.dll next to the target is not the one of the build")
endif ()
file(REMOVE_RECURSE "${WORK}")
