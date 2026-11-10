//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin whose build id function returns null, which a plugin written by
// hand can do. Loading it has to fail with a diagnostic instead of
// dereferencing the null pointer. The ctest entry checks that the run
// fails, and checks the diagnostic to confirm that it fails for this
// reason.

#include <mrdocs/Plugin.hpp>

extern "C" MRDOCS_PLUGIN_EXPORT char const*
mrdocs_plugin_build_id()
{
    return nullptr;
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
