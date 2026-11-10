//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin whose entry point returns false and leaves the error empty. The
// run has to stop and say the plugin reported a failure, since there is no
// reason to name.
//
// The three functions are written out rather than defined with
// `MRDOCS_PLUGIN_MAIN`, whose entry point always leaves an error behind when
// the body fails.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_build_id()
{
    return MRDOCS_PLUGIN_BUILD_ID;
}

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_stdlib_tag()
{
    return MRDOCS_PLUGIN_STDLIB_TAG;
}

extern "C" MRDOCS_PLUGIN_EXPORT bool
mrdocs_plugin_main(mrdocs::PluginContext&, mrdocs::Error*)
{
    return false;
}
