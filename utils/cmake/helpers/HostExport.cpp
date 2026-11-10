//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin host on Windows has to export something, or the linker writes no
// import library for it and CMake cannot link a plugin against it. See
// mrdocs_target_plugin_host_compile_definitions
// in helpers/mrdocs_target_plugin_host_compile_definitions.cmake.
extern "C" __declspec(dllexport) void mrdocs_plugin_host_export() {}
