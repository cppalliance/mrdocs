//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "HandlebarsCorpus.hpp"
#include "VisitorHelpers.hpp"
#include <mrdocs/Support/Container/RangeFor.hpp>
#include <mrdocs/Support/String/String.hpp>
#include <iterator>

namespace mrdocs::hbs {

dom::Object
HandlebarsCorpus::
construct(Symbol const& I) const
{
    // A pure reflection view of the C++ object
    return this->DomCorpus::construct(I);
}

std::string
HandlebarsCorpus::
getURL(Symbol const& I) const
{
    if (shouldGenerate(I, config()))
    {
        return getCanonicalURL(I);
    }

    // If the URL is not available because it's a specialization
    // or dependency, use the URL of the primary template it is
    // documented under, when that is part of the corpus.
    if (Symbol const* primaryInfo =
            findAlternativeURLInfo(getCorpus(), config(), I))
    {
        MRDOCS_ASSERT(shouldGenerate(*primaryInfo, config()));
        return getCanonicalURL(*primaryInfo);
    }
    return {};
}

namespace {

// Assemble a symbol's qualified legible name, joined with `delim`, from
// the per-symbol unqualified `Anchor` values the AnchorFinalizer stored on
// I and its ancestors. Each ancestor up to (but excluding) the global
// namespace contributes its own anchor: '/' joins them into a multipage
// file path, '-' into a single-page fragment.
void
appendQualifiedAnchor(
    Corpus const& corpus,
    Symbol const& I,
    char const delim,
    std::string& out)
{
    if (I.Parent != SymbolID::invalid && I.Parent != SymbolID::global)
    {
        if (Symbol const* parent = corpus.find(I.Parent))
        {
            appendQualifiedAnchor(corpus, *parent, delim, out);
            out.push_back(delim);
        }
    }
    out.append(I.Anchor);
}

// The exact length appendQualifiedAnchor will produce: the same walk,
// summing per-symbol anchor sizes plus one delimiter per join, so a caller
// can reserve the result up front.
std::size_t
qualifiedAnchorLength(Corpus const& corpus, Symbol const& I)
{
    std::size_t len = I.Anchor.size();
    if (I.Parent != SymbolID::invalid && I.Parent != SymbolID::global)
    {
        if (Symbol const* parent = corpus.find(I.Parent))
        {
            len += 1 + qualifiedAnchorLength(corpus, *parent);
        }
    }
    return len;
}

} // (anon)

std::string
HandlebarsCorpus::
getQualifiedAnchor(Symbol const& I, char const delim) const
{
    // A page-unique reference for the symbol: its qualified legible name,
    // walking the parent chain and joining per-symbol anchors with `delim`.
    // getCanonicalURL uses this with '/' (multipage paths) or '-'
    // (single-page fragments); templates use the '-' form for heading ids.
    // A hashed anchor is already unique on its own, so it stays flat.
    if (!config().legibleNames)
    {
        return I.Anchor;
    }
    std::string out;
    out.reserve(qualifiedAnchorLength(getCorpus(), I));
    appendQualifiedAnchor(getCorpus(), I, delim, out);
    return out;
}

std::string
HandlebarsCorpus::
getCanonicalURL(Symbol const& I) const
{
    bool const multipage = config().multipage;
    std::string const anchor = getQualifiedAnchor(I, multipage ? '/' : '-');
    std::string href;
    href.reserve(1 + anchor.size() + (multipage ? 1 + fileExtension.size() : 0));
    href += multipage ? '/' : '#';
    href += anchor;
    if (multipage)
    {
        href += '.';
        href += fileExtension;
    }
    return href;
}

} // mrdocs::hbs
