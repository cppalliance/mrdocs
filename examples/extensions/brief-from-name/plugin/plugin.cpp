//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The C++ version of the brief-from-name transform on the Corpus Transforms
// page: a function named `is_<name>` gets the brief "Returns true if <name>."
// and a description for its single parameter.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Transform.hpp>
#include <algorithm>
#include <string>
#include <string_view>

namespace {

// tag::fill-brief[]
void
fillBrief(mrdocs::Symbol& symbol, std::string const& subject)
{
    if (!symbol.doc)
    {
        symbol.doc.emplace();
    }
    if (!symbol.doc->brief)
    {
        symbol.doc->brief = mrdocs::doc::BriefBlock(
            "Returns true if " + subject + ".");
    }
}
// end::fill-brief[]

// tag::fill-param[]
void
fillParam(mrdocs::Symbol& symbol, mrdocs::FunctionSymbol const& function,
    std::string const& subject)
{
    auto const& name = function.Params[0].Name;
    symbol.doc->params.emplace_back(name ? *name : std::string(),
        "The input examined for the " + subject + " property.");
}
// end::fill-param[]

// tag::describe[]
void
describePredicate(mrdocs::Corpus& corpus, mrdocs::Symbol const& symbol)
{
    auto const* function = symbol.asFunctionPtr();
    if (!function || !symbol.Name.starts_with("is_"))
    {
        return;
    }
    std::string subject = symbol.Name.substr(3);
    std::ranges::replace(subject, '_', ' ');
    mrdocs::Symbol* const target = corpus.find(symbol.id);
    fillBrief(*target, subject);
    if (function->Params.size() == 1 && target->doc->params.empty())
    {
        fillParam(*target, *function, subject);
    }
}
// end::describe[]

} // (anon)

MRDOCS_PLUGIN_MAIN(plugin)
{
// tag::transform[]
    return plugin.registerTransform("brief-from-name",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            for (mrdocs::Symbol const& symbol : *ctx.corpus)
            {
                describePredicate(*ctx.corpus, symbol);
            }
            return {};
        });
// end::transform[]
}
