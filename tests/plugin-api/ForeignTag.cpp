//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin that reports a toolchain this MrDocs was not built with, which is
// what a plugin compiled by another compiler, or against another standard
// library, looks like. Loading it has to fail before its entry point is
// called; the ctest entry checks that the run fails, and by the diagnostic
// that it fails for this reason.
//
// The three functions are written out rather than defined with
// `MRDOCS_PLUGIN_MAIN`, since that macro reports the toolchain the plugin is
// really built with, which is by construction the accepted one.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT int
mrdocs_plugin_api_version()
{
    return MRDOCS_PLUGIN_API_VERSION;
}

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_build_tag()
{
    return "foreign-toolchain";
}

extern "C" MRDOCS_PLUGIN_EXPORT bool
mrdocs_plugin_main(mrdocs::PluginContext&, mrdocs::Error*)
{
    // Not reached: the toolchain is checked before the entry point is even
    // looked up. Installing nothing and reporting a failure keeps the test
    // meaningful if that order ever changes.
    return false;
}
