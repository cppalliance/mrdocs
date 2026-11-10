//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A library with none of the entry points, like a dependency of a plugin
// that someone put directly in the extensions directory instead of in the
// directory of the plugin that uses it. The loader takes every library there
// for a plugin, so it has to refuse this one at the first function it looks
// for.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT int
mrdocs_test_not_a_plugin()
{
    return 1;
}
