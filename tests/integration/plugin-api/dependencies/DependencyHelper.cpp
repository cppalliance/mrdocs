//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A shared library a plugin depends on. Where the ctest entries put it decides
// what they test. Next to the plugin, in its directory under `extensions`, the
// loader has to find it. On Windows, in a directory reachable only through
// PATH, the loader must not find it, because it must not consult PATH when it
// resolves a plugin's dependencies.

#if defined(_WIN32)
#define MRDOCS_TEST_HELPER_EXPORT __declspec(dllexport)
#else
#define MRDOCS_TEST_HELPER_EXPORT \
    __attribute__((__visibility__("default")))
#endif

extern "C" MRDOCS_TEST_HELPER_EXPORT int
mrdocs_test_dependency_helper()
{
    return 42;
}
