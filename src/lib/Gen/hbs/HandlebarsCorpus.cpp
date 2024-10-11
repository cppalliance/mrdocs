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
#include <mrdocs/ADT/Polymorphic.hpp>
#include <iterator>
#include <fmt/format.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_ostream.h>
#include <mrdocs/Support/RangeFor.hpp>
#include <mrdocs/Support/String.hpp>

namespace clang::mrdocs::hbs {

namespace {

dom::Value
domCreate(
    doc::Param const& I,
    HandlebarsCorpus const& corpus)
{
    dom::Object::storage_type entries = {
        { "name", I.name }
    };
    if (std::string s = corpus.toStringFn(corpus, I); !s.empty())
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
    doc::TParam const& I,
    HandlebarsCorpus const& corpus)
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
    doc::Throws const& I,
    HandlebarsCorpus const& corpus)
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
    doc::See const& I,
    HandlebarsCorpus const& corpus)
{
    return corpus.toStringFn(corpus, I);
}

dom::Value
domCreate(
    doc::Related const& I,
    HandlebarsCorpus const& corpus)
{
    return corpus.toStringFn(corpus, I);
}

dom::Value
domCreate(
    doc::Precondition const& I,
    HandlebarsCorpus const& corpus)
{
    return corpus.toStringFn(corpus, I);
}

dom::Value
domCreate(
    doc::Postcondition const& I,
    HandlebarsCorpus const& corpus)
{
    return corpus.toStringFn(corpus, I);
}

dom::Value
domCreate(
    doc::Returns const& I,
    HandlebarsCorpus const& corpus)
{
    return corpus.toStringFn(corpus, I);
}

} // (anon)

dom::Object
HandlebarsCorpus::
construct(Info const& I) const
{
    dom::Object obj = this->DomCorpus::construct(I);
    if (shouldGenerate(I, getCorpus().config))
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
        MRDOCS_ASSERT(shouldGenerate(*primaryInfo, getCorpus().config));
        obj.set("url", getURL(*primaryInfo));
        obj.set("anchor", names_.getQualified(primaryInfo->id, '-'));
    }
    return obj;
}

std::string
HandlebarsCorpus::
getURL(Info const& I) const
{
    bool const multipage = getCorpus().config->multipage;
    char const prefix = multipage ? '/' : '#';
    char const delim = multipage ? '/' : '-';
    std::string href(1, prefix);
    href += names_.getQualified(I.id, delim);
    if (multipage)
    {
        href.append(".");
        href.append(fileExtension);
    }
    return href;
}

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
    auto emplaceString = [&]<typename T>(
        std::string_view key,
        T const& I)
    {
        std::string s;
        if constexpr (std::derived_from<T, doc::Node>)
        {
            s += toStringFn(*this, I);
        }
        else if constexpr (std::ranges::range<T>)
        {
            using value_type = std::ranges::range_value_t<T>;
            if constexpr (IsPolymorphic_v<value_type>)
            {
                for (value_type const& v: I)
                {
                    auto const& node = dynamic_cast<doc::Node const&>(*v);
                    s += toStringFn(*this, node);
                }
            }
            else
            {
                for (doc::Node const* t: I)
                {
                    s += toStringFn(*this, *t);
                }
            }
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
    auto emplaceObjectArray = [&]<class Range>(
        std::string_view key,
        Range const& nodes)
    {
        using value_type = std::ranges::range_value_t<Range>;
        dom::Array::storage_type elements;
        elements.reserve(nodes.size());
        for (value_type const& elem : nodes)
        {
            if constexpr (requires { !elem; })
            {
                if (!elem)
                {
                    continue;
                }
                elements.emplace_back(domCreate(*elem, *this));
            }
            else
            {
                elements.emplace_back(domCreate(elem, *this));
            }
        }
        if (elements.empty())
        {
            return;
        }
        objKeyValues.emplace_back(key, dom::newArray<
            dom::DefaultArrayImpl>(std::move(elements)));
    };

    // brief
    if (jd.brief)
    {
        emplaceString("brief", *jd.brief);
    }
    emplaceString("description", jd.blocks);
    emplaceObjectArray("returns", jd.returns);
    emplaceObjectArray("params", jd.params);
    emplaceObjectArray("tparams", jd.tparams);
    emplaceObjectArray("exceptions", jd.exceptions);
    emplaceObjectArray("see", jd.sees);
    emplaceObjectArray("related", jd.related);
    emplaceObjectArray("preconditions", jd.preconditions);
    emplaceObjectArray("postconditions", jd.postconditions);
    return dom::Object(std::move(objKeyValues));
}

} // clang::mrdocs::hbs
