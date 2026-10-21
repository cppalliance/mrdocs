//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin that reports an interface version and a toolchain this MrDocs
// accepts, and exports no entry point, which is what a library written
// without `MRDOCS_PLUGIN_MAIN`, or with the entry point misspelled, looks
// like. Loading it has to fail; the ctest entry checks that the run fails,
// and by the diagnostic that it fails for this reason.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT int
mrdocs_plugin_api_version()
{
    return MRDOCS_PLUGIN_API_VERSION;
}

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_build_tag()
{
    return MRDOCS_PLUGIN_BUILD_TAG;
}
