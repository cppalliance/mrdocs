//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The hello-world transform of the Defining Extensions page, the same
// extension as its JavaScript and Lua tabs: it reports one line.

// tag::plugin[]
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>

MRDOCS_PLUGIN_MAIN(plugin)
{
    return plugin.registerTransform("hello-world-transform",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            ctx.report->warn("Hello from a transform");
            return {};
        });
}
// end::plugin[]
