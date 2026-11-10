#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# mrdocs_ensure_runtime_dlls(<target>)
#
# Make the MrDocs DLLs <target> needs findable next to <target> on Windows, when
# `mrdocs::mrdocs-core` is a shared library.
#
# Examples
#
#   Suppose MrDocs is a shared library on Windows, and `foo` links it:
#
#     add_executable(foo main.cpp)
#     target_link_libraries(foo PRIVATE mrdocs::mrdocs-core)
#     mrdocs_ensure_runtime_dlls(foo)
#
#   When `foo` is built, `foo.exe` and `mrdocs-core.dll` end up side by side in
#   one directory, so `foo.exe` starts without any change to PATH. The function
#   does not copy the libraries behind MrDocs, such as LLVM, because they are
#   static archives inside `mrdocs-core.dll`.
#
#   If `foo` also links `bar`, a shared library of the same project, `bar.dll`
#   is put next to `foo.exe` too:
#
#     target_link_libraries(foo PRIVATE mrdocs::mrdocs-core bar)
#     mrdocs_ensure_runtime_dlls(foo)
#
#   A DLL is skipped when a file with the same name and the same SHA256 is
#   already in the directory of `foo.exe` or in a directory on PATH. If
#   `C:\mrdocs\bin` is on PATH and holds the same `mrdocs-core.dll`, the build
#   puts only `bar.dll` next to `foo.exe`.
#
#   If MrDocs is a static library, the same lines copy nothing, not even
#   `bar.dll`, because the function returns before it adds the step.
#
#   On Linux and macOS the same lines do nothing. The linker records where
#   `libmrdocs-core.so` or `libmrdocs-core.dylib` is, and the program finds it
#   there.
#
# Windows finds a DLL next to the program or on PATH, and rpath does not exist
# there. The function also covers the other DLLs <target> needs, such as those
# of its own project.
#
# Parameters
#
#   <target>  An executable or a library that links the shared libraries it
#             needs at run time.
#
# Preconditions
#
#   The function needs CMake 3.21 or later, for TARGET_RUNTIME_DLLS. Call it
#   after <target> links what it needs, since the DLLs are those of its link
#   graph.
#
# Guarantees
#
#   The function adds a post-build step to <target>. The step looks at every
#   DLL <target> needs. It does nothing when a file of the same name and SHA256
#   is in the directory of <target> or on PATH. Otherwise it puts a symbolic
#   link to the DLL next to <target>, or a copy when the account cannot link.
#   The function does nothing on other platforms, and nothing when
#   `mrdocs::mrdocs-core` is missing or a static library.
#
#   The check sees PATH as it is when the build runs. A program that is later
#   run without that directory on PATH does not find a DLL that was skipped
#   because of it.
function(mrdocs_ensure_runtime_dlls target)
    if (NOT WIN32 OR NOT TARGET mrdocs::mrdocs-core)
        return()
    endif ()
    get_target_property(mrdocs_core_type mrdocs::mrdocs-core TYPE)
    if (NOT mrdocs_core_type STREQUAL "SHARED_LIBRARY")
        return()
    endif ()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D "DLLS=$<TARGET_RUNTIME_DLLS:${target}>"
            -D "DIR=$<TARGET_FILE_DIR:${target}>"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/LinkOrCopyDlls.cmake"
        VERBATIM)
endfunction()
