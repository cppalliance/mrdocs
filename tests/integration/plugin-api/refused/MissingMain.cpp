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

// A plugin that reports a MrDocs build and a standard library this MrDocs
// accepts, and exports no entry point. That is what a library written
// without `MRDOCS_PLUGIN_MAIN`, or with the entry point misspelled, looks
// like. Loading it has to fail. The ctest entry checks that the run fails,
// and checks the diagnostic to confirm that it fails for this reason.

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
