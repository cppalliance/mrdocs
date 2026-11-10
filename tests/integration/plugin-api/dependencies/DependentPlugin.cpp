//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin that links a shared library of its own. Its entry point calls the
// library and fails unless the library returns its value, so a run that loads
// the plugin shows the library was found and called. The ctest entries decide
// where the library is. In the subdirectory named after the plugin, loading
// has to succeed. On Windows, in PATH only, which the loader does not search,
// loading has to fail.

#include <mrdocs/Plugin.hpp>

#if defined(_WIN32)
#define MRDOCS_TEST_HELPER_IMPORT __declspec(dllimport)
#else
#define MRDOCS_TEST_HELPER_IMPORT
#endif

extern "C" MRDOCS_TEST_HELPER_IMPORT int mrdocs_test_dependency_helper();

MRDOCS_PLUGIN_MAIN(plugin)
{
    (void)plugin;
    if (mrdocs_test_dependency_helper() != 42)
    {
        return mrdocs::Unexpected(mrdocs::formatError(
            "the dependency of the plugin returned the wrong value"));
    }
    return {};
}
