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

// A MrDocs plugin: a shared library that MrDocs loads as it starts up. This
// one registers a transform, which gives every symbol without a brief a
// placeholder one. The transform is a function registered under an id.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Transform.hpp>
#include <string>

namespace {

// Give a symbol a brief with the given text if it has none.
void
fillBrief(mrdocs::Symbol& symbol, std::string const& text)
{
    if (!symbol.doc)
    {
        symbol.doc.emplace();
    }
    mrdocs::DocComment& doc = symbol.doc.value();
    if (!doc.brief)
    {
        doc.brief = mrdocs::doc::BriefBlock(text);
    }
}

// A transform that gives a symbol with no brief a placeholder one, so that
// the gap shows up in the output instead of a blank space. The placeholder
// comes from the `text` option of this transform's `transform-options` block,
// if the configuration has one.
mrdocs::Expected<void>
fillBriefs(mrdocs::TransformExtensionContext const& ctx)
{
    std::string text = "Undocumented.";
    if (ctx.params.exists("text") && ctx.params.get("text").isString())
    {
        text = std::string(ctx.params.get("text").getString());
    }
    // A corpus is walked as const. Changing a symbol means asking for it
    // by id, which is where the corpus hands out a mutable reference.
    for (mrdocs::Symbol const& symbol : *ctx.corpus)
    {
        if (mrdocs::Symbol* const target = ctx.corpus->find(symbol.id))
        {
            fillBrief(*target, text);
        }
    }
    return {};
}

} // (anon)

MRDOCS_PLUGIN_MAIN(plugin)
{
    return plugin.registerTransform("brief-filler", fillBriefs);
}
