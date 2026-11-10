//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin that reports a standard library this MrDocs was not built with,
// which is what a plugin compiled against another standard library, or
// configured another way, looks like. Loading it has to fail before its
// entry point is called. The ctest entry checks that the run fails, and
// checks the diagnostic to confirm that it fails for this reason.
//
// The three functions are written out rather than defined with
// `MRDOCS_PLUGIN_MAIN`, since that macro reports the standard library the
// plugin is really built with, which is by construction the accepted one.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_build_id()
{
    return MRDOCS_PLUGIN_BUILD_ID;
}

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_stdlib_tag()
{
    return "foreign-stdlib";
}

extern "C" MRDOCS_PLUGIN_EXPORT bool
mrdocs_plugin_main(mrdocs::PluginContext&, mrdocs::Error*)
{
    // Not reached: the standard library is checked before the entry point
    // is even looked up. Installing nothing and reporting a failure keeps
    // the test meaningful if that order ever changes.
    return false;
}
