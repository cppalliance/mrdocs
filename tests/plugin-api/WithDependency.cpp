//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin with a shared library of its own, kept where a plugin's
// dependencies go: the subdirectory of the plugins directory named after
// the plugin. Loading the plugin shows that the library is found there. The
// entry point then fails on purpose, with the value the library returned,
// so that the run stops before it needs any input; the ctest entry checks
// by the diagnostic that the library answered and that the error names the
// plugin.

#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>

// Defined in Dependency.cpp, which is built into a library of its own.
int
pluginDependencyAnswer();

MRDOCS_PLUGIN_MAIN(context)
{
    static_cast<void>(context);
    return mrdocs::Unexpected(mrdocs::formatError(
        "the dependency answered {}", pluginDependencyAnswer()));
}
