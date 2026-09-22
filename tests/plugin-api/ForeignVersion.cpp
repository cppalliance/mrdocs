//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin built against an interface this MrDocs does not provide, which
// is what a plugin left over from an earlier MrDocs looks like. Loading it
// has to fail, and the run has to stop before anything is extracted; the
// ctest entry checks that by the diagnostic.
//
// The three functions are written out rather than defined with
// `MRDOCS_PLUGIN_MAIN`, since that macro reports the version the headers
// it is expanded from define, which is by construction the accepted one.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT int
mrdocs_plugin_api_version()
{
    return MRDOCS_PLUGIN_API_VERSION + 1;
}

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_build_tag()
{
    return MRDOCS_PLUGIN_BUILD_TAG;
}

extern "C" MRDOCS_PLUGIN_EXPORT bool
mrdocs_plugin_main(mrdocs::PluginContext&, mrdocs::Error*)
{
    // Not reached: the version is checked before the entry point is even
    // looked up. Installing nothing and reporting a failure keeps the test
    // meaningful if that order ever changes.
    return false;
}
