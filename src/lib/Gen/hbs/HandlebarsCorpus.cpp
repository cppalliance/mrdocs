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
#include <iterator>
#include <fmt/format.h>
#include <llvm/Support/raw_ostream.h>
#include <mrdocs/Support/RangeFor.hpp>
#include <mrdocs/Support/String.hpp>

namespace clang {
namespace mrdocs {
namespace hbs {

namespace {

dom::Value
domCreate(
    const doc::Param& I,
    const HandlebarsCorpus& corpus)
{
    dom::Object::storage_type entries = {
        { "name", I.name }
    };
    std::string s = corpus.toStringFn(corpus, I);
    if (!s.empty())
    {
        entries.emplace_back(
            "description", std::move(s));
    }
    // Direction
    if (I.direction == doc::ParamDirection::in)
    {
        entries.emplace_back(
            "direction", "in");
    }
    else if (I.direction == doc::ParamDirection::out)
    {
        entries.emplace_back(
            "direction", "out");
    }
    else if (I.direction == doc::ParamDirection::inout)
    {
        entries.emplace_back(
            "direction", "inout");
    }
    return dom::Object(std::move(entries));
}

dom::Value
domCreate(
    const doc::TParam& I,
    const HandlebarsCorpus& corpus)
{
    dom::Object::storage_type entries = {
        { "name", I.name }
    };
    std::string s = corpus.toStringFn(corpus, I);
    if (!s.empty())
    {
        entries.emplace_back(
            "description", std::move(s));
    }
    return dom::Object(std::move(entries));
}

dom::Value
domCreate(
    const doc::Throws& I,
    const HandlebarsCorpus& corpus)
{
    dom::Object::storage_type entries = {
        { "exception", I.exception.string }
    };
    std::string s = corpus.toStringFn(corpus, I);
    if (!s.empty())
    {
        entries.emplace_back(
            "description", std::move(s));
    }
    return dom::Object(std::move(entries));
}

dom::Value
domCreate(
    const doc::See& I,
    const HandlebarsCorpus& corpus)
{
    return corpus.toStringFn(corpus, I);
}

dom::Value
domCreate(
    const doc::Precondition& I,
    const HandlebarsCorpus& corpus)
{
    return corpus.toStringFn(corpus, I);
}

dom::Value
domCreate(
    const doc::Postcondition& I,
    const HandlebarsCorpus& corpus)
{
    return corpus.toStringFn(corpus, I);
}

} // (anon)

dom::Object
HandlebarsCorpus::
construct(Info const& I) const
{
    dom::Object obj = this->DomCorpus::construct(I);
    if (shouldGenerate(I))
    {
        obj.set("url", getURL(I));
        obj.set("anchor", names_.getQualified(I.id, '-'));
        return obj;
    }

    // If the URL is not available because it's a specialization
    // or dependency, we still want to generate the URL and anchor
    // for the primary template if it's part of the corpus.
    if (Info const* primaryInfo = findAlternativeURLInfo(getCorpus(), I))
    {
        obj.set("url", getURL(*primaryInfo));
        obj.set("anchor", names_.getQualified(primaryInfo->id, '-'));
    }
    return obj;
}

dom::Object
HandlebarsCorpus::
construct(
    OverloadSet const& I) const
{
    auto obj = this->DomCorpus::construct(I);
    obj.set("url", getURL(I));
    obj.set("anchor", names_.getQualified(I, '-'));
    return obj;
}

template<class T>
requires std::derived_from<T, Info> || std::same_as<T, OverloadSet>
std::string
HandlebarsCorpus::
getURL(T const& I) const
{
    bool const multipage = getCorpus().config->multipage;
    char const prefix = multipage ? '/' : '#';
    char const delim = multipage ? '/' : '-';
    std::string href(1, prefix);
    if constexpr (std::derived_from<T, Info>)
    {
        href += names_.getQualified(I.id, delim);
    }
    else if constexpr (std::same_as<T, OverloadSet>)
    {
        href += names_.getQualified(I, delim);
    }
    if (multipage)
    {
        href.append(".");
        href.append(fileExtension);
    }
    return href;
}

// Define Builder::operator() for each Info type
#define INFO(T) template std::string HandlebarsCorpus::getURL<T## Info>(T## Info const&) const;
#include <mrdocs/Metadata/InfoNodesPascal.inc>

template std::string HandlebarsCorpus::getURL<OverloadSet>(OverloadSet const&) const;


dom::Value
HandlebarsCorpus::
getJavadoc(Javadoc const& jd) const
{
    dom::Object::storage_type objKeyValues;
    objKeyValues.reserve(2);

    /* Emplace the string value representing the
       Javadoc node.

       When the string is empty, the object key
       is undefined.
     */
    auto emplaceString = [&](
        std::string_view key,
        auto const& I)
    {
        std::string s;
        using T = std::decay_t<decltype(I)>;
        if constexpr (std::derived_from<T, doc::Node>)
        {
            // doc::visit(*t, visitor);
            s += toStringFn(*this, I);
        }
        else if constexpr (std::ranges::range<T>)
        {
            // Range value type
            for(doc::Node const* t : I)
                s += toStringFn(*this, *t);
        }
        if(! s.empty())
        {
            objKeyValues.emplace_back(key, std::move(s));
        }
    };

    /* Emplace an array of objects where each element
       represents the properties of the node type,
       such as "name" and "description".
     */
    auto emplaceObjectArray = [&](
        std::string_view key,
        /* std::vector<T const*> */ auto const& nodes)
    {
        dom::Array::storage_type elements;
        elements.reserve(nodes.size());
        for(auto const& elem : nodes)
        {
            if(!elem)
                continue;
            elements.emplace_back(
                domCreate(*elem, *this));
        }
        if(elements.empty())
            return;
        objKeyValues.emplace_back(key, dom::newArray<
            dom::DefaultArrayImpl>(std::move(elements)));
    };

    auto ov = jd.makeOverview(this->getCorpus());
    // brief
    if(ov.brief)
        emplaceString("brief", *ov.brief);
    emplaceString("description", ov.blocks);
    if(ov.returns)
        emplaceString("returns", *ov.returns);
    emplaceObjectArray("params", ov.params);
    emplaceObjectArray("tparams", ov.tparams);
    emplaceObjectArray("exceptions", ov.exceptions);
    emplaceObjectArray("see", ov.sees);
    emplaceObjectArray("preconditions", ov.preconditions);
    emplaceObjectArray("postconditions", ov.postconditions);
    return dom::Object(std::move(objKeyValues));
}

} // hbs
} // mrdocs
} // clang
