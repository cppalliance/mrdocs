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
#include "lib/Support/Radix.hpp"
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
        { "exception", I.exception }
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
    obj.set("ref", getXref(I));
    return obj;
}

std::string
HandlebarsCorpus::
getXref(Info const& I) const
{
    bool multipage = getCorpus().config->multipage;
    // use '/' as the seperator for multipage, and '-' for single-page
    std::string xref;
    if(!multipage)
    {
        xref += "#";
    }
    char delim = multipage ? '/' : '-';
    xref += names_.getQualified(I.id, delim);
    // add the file extension if in multipage mode
    if(multipage)
    {
        xref.append(".");
        xref.append(fileExtension);
    }
    return xref;
}

std::string
HandlebarsCorpus::
getXref(OverloadSet const& os) const
{
    bool multipage = getCorpus().config->multipage;
    // use '/' as the seperator for multipage, and '-' for single-page
    std::string xref = names_.getQualified(
        os, multipage ? '/' : '-');
    // add the file extension if in multipage mode
    if(multipage)
    {
        xref.append(".");
        xref.append(fileExtension);
    }
    return xref;
}

dom::Value
HandlebarsCorpus::
getJavadoc(
    Javadoc const& jd) const
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

dom::Object
HandlebarsCorpus::
getOverloads(
    OverloadSet const& os) const
{
    auto obj = DomCorpus::getOverloads(os);
    // KRYSTIAN FIXME: need a better way to generate IDs
    // std::string xref =
    obj.set("ref", getXref(os));
    // std::replace(xref.begin(), xref.end(), '/', '-');
    obj.set("id", fmt::format("{}-{}",
        toBase16(os.Parent), os.Name));
    return obj;
}

} // hbs
} // mrdocs
} // clang
