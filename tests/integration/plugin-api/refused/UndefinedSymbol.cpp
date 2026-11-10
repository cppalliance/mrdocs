//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin that refers to a function nothing provides, which is what a
// plugin calling something its MrDocs lacks looks like. The loader resolves
// every symbol when it opens a library, so the library has to be refused
// right there, and the message has to name the function. The ctest entry
// checks that.

#include <mrdocs/Plugin.hpp>

extern "C" int mrdocs_test_symbol_nothing_provides();

MRDOCS_PLUGIN_MAIN(plugin)
{
    (void)plugin;
    mrdocs_test_symbol_nothing_provides();
    return {};
}
