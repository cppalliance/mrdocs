//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin for the test that loads the same plugins into a process more than
// once, see ReloadHost.cpp. Its entry point registers a generator, which fails
// when the entry point runs a second time because the id is taken. It also
// registers, in the callable form, a transform that appends an X to the name of every symbol named
// Widget, so the name says how many times the transform ran.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Transform.hpp>

MRDOCS_PLUGIN_MAIN(plugin)
{
    MRDOCS_TRY(plugin.registerGenerator(
        "reload-test",
        [](mrdocs::GeneratorExtensionContext const&) -> mrdocs::Expected<void>
        {
            return {};
        },
        "Reload test",
        "txt"));
    return plugin.registerTransform("reload-append",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            for (mrdocs::Symbol const& symbol : *ctx.corpus)
            {
                if (symbol.Name.starts_with("Widget"))
                {
                    ctx.corpus->find(symbol.id)->Name += "X";
                }
            }
            return {};
        });
}
