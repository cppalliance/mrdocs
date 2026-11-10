#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# A -P script: for each DLL in the list DLLS, unless a file with the same name
# and SHA256 is already in the directory DIR or in a directory on PATH, puts a
# symbolic link to the DLL into DIR, and copies the DLL when the link cannot be
# made. The function mrdocs_ensure_runtime_dlls in
# mrdocs_ensure_runtime_dlls.cmake runs it after a build.

# PATH splits at its native separator: ';' on Windows, where a ':' belongs to a
# drive, and ':' elsewhere. Empty entries are dropped below. On Windows the
# backslashes become slashes.
if (CMAKE_HOST_WIN32)
    set(path_dirs "$ENV{PATH}")
    string(REPLACE "\\" "/" path_dirs "${path_dirs}")
else ()
    string(REPLACE ":" ";" path_dirs "$ENV{PATH}")
endif ()
set(search_dirs "${DIR}")
foreach (path_dir IN LISTS path_dirs)
    if (path_dir)
        list(APPEND search_dirs "${path_dir}")
    endif ()
endforeach ()

foreach (dll IN LISTS DLLS)
    get_filename_component(name "${dll}" NAME)
    file(SIZE "${dll}" dll_size)
    file(SHA256 "${dll}" dll_hash)
    set(found FALSE)
    foreach (search_dir IN LISTS search_dirs)
        set(candidate "${search_dir}/${name}")
        if (EXISTS "${candidate}")
            file(SIZE "${candidate}" candidate_size)
        endif ()
        if (EXISTS "${candidate}" AND candidate_size EQUAL dll_size)
            file(SHA256 "${candidate}" candidate_hash)
            if (candidate_hash STREQUAL dll_hash)
                set(found TRUE)
                break()
            endif ()
        endif ()
    endforeach ()
    if (NOT found)
        file(CREATE_LINK "${dll}" "${DIR}/${name}" SYMBOLIC COPY_ON_ERROR)
    endif ()
endforeach ()
