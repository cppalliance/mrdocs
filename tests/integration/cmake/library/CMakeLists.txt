#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Links the core library into a program, as a user of the library does.

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE mrdocs::mrdocs-core)
mrdocs_ensure_runtime_dlls(consumer)
target_compile_features(consumer PRIVATE cxx_std_23)
add_test(NAME consumer-runs COMMAND consumer)

# The post-build script of mrdocs_ensure_runtime_dlls skips a DLL that is already
# in the target's directory or on PATH with the same content. The check is made
# with the installed copy of the script and made-up files, so it runs on every
# platform.
add_test(NAME consumer-runtime-dlls-skip-identical
    COMMAND ${CMAKE_COMMAND}
        "-DSCRIPT=${mrdocs_DIR}/LinkOrCopyDlls.cmake"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/runtime-dlls"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckLinkOrCopyDlls.cmake")

get_property(consumer_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${consumer_tests} PROPERTIES LABELS integration)
