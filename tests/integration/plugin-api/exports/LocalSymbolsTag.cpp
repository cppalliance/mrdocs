//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The function LocalSymbols.cpp calls. Every library built from this file
// exports one of the same name, which returns the tag it was built with.

#include <mrdocs/Plugin.hpp>

#ifndef MRDOCS_TEST_TAG
#error "MRDOCS_TEST_TAG names the library"
#endif

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_test_tag()
{
    return MRDOCS_TEST_TAG;
}
