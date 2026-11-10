//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The C++ version of the parse-format-relates transform on the Corpus
// Transforms page: each `parse_X` and `format_X` function gets a See Also
// link to its partner.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Transform.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace {

// tag::partner-name[]
std::string
partnerName(std::string_view name)
{
    if (name.starts_with("parse_"))
    {
        return "format_" + std::string(name.substr(6));
    }
    if (name.starts_with("format_"))
    {
        return "parse_" + std::string(name.substr(7));
    }
    return {};
}
// end::partner-name[]

// tag::add-see[]
void
addSee(mrdocs::Symbol& target, std::string const& name,
    mrdocs::SymbolID const& id)
{
    mrdocs::doc::ReferenceInline reference(name);
    reference.id = id;
    mrdocs::doc::SeeBlock see;
    see.children.emplace_back(std::move(reference));
    if (!target.doc)
    {
        target.doc.emplace();
    }
    target.doc->sees.clear();
    target.doc->sees.push_back(std::move(see));
}
// end::add-see[]

// tag::relate[]
void
relatePartner(mrdocs::Corpus& corpus, mrdocs::Symbol const& symbol)
{
    if (!symbol.isFunction())
    {
        return;
    }
    std::string const name = partnerName(symbol.Name);
    if (name.empty())
    {
        return;
    }
    auto const partner = corpus.lookup(name);
    if (partner)
    {
        addSee(*corpus.find(symbol.id), name, partner->id);
    }
}
// end::relate[]

} // (anon)

MRDOCS_PLUGIN_MAIN(plugin)
{
// tag::transform[]
    return plugin.registerTransform("parse-format-relates",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            for (mrdocs::Symbol const& symbol : *ctx.corpus)
            {
                relatePartner(*ctx.corpus, symbol);
            }
            return {};
        });
// end::transform[]
}
