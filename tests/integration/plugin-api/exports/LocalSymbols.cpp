//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin whose symbols have to stay its own. Built twice, with a different
// MRDOCS_TEST_TAG, it exports a function of the same name from each library.
// The loader opens libraries so that their symbols are not offered to the
// libraries loaded later, so the entry point of each one has to reach its own
// function. A loader that made the symbols global would let the library that
// loads second call the function of the first, and its entry point would
// fail.

#include <mrdocs/Plugin.hpp>
#include <string_view>

#ifndef MRDOCS_TEST_TAG
#error "MRDOCS_TEST_TAG names the library"
#endif

// Defined in LocalSymbolsTag.cpp, a translation unit of its own. The call in
// the entry point has to stay a call through the symbol table of the
// library. A compiler that sees the body folds the call into the tag, and
// the test could not tell a loader that kept the symbols local from one that
// did not.
extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_test_tag();

MRDOCS_PLUGIN_MAIN(plugin)
{
    (void)plugin;
    if (std::string_view(mrdocs_test_tag()) != MRDOCS_TEST_TAG)
    {
        return mrdocs::Unexpected(mrdocs::formatError(
            "local-symbols: the call reached the function of another library"));
    }
    return {};
}
