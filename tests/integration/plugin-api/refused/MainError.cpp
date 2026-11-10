//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin whose entry point fails with a message. The run has to stop and
// name the library and the reason the entry point gave.

#include <mrdocs/Plugin.hpp>

MRDOCS_PLUGIN_MAIN(plugin)
{
    (void)plugin;
    return mrdocs::Unexpected(
        mrdocs::formatError("the entry point gave up"));
}
