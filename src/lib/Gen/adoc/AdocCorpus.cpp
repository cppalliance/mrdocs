//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "AdocCorpus.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdocs/Support/RangeFor.hpp>
#include <mrdocs/Support/String.hpp>
#include <fmt/format.h>
#include <iterator>

#include <llvm/Support/raw_ostream.h>

namespace clang {
namespace mrdocs {
namespace adoc {

namespace {

class DocVisitor
{
    const AdocCorpus& corpus_;
    std::string& dest_;
    std::back_insert_iterator<std::string> ins_;

public:
    DocVisitor(
        const AdocCorpus& corpus,
        std::string& dest) noexcept
        : corpus_(corpus)
        , dest_(dest)
        , ins_(std::back_inserter(dest_))
    {
    }

    void operator()(doc::Admonition const& I);
    void operator()(doc::Code const& I);
    void operator()(doc::Heading const& I);
    void operator()(doc::Paragraph const& I);
    void operator()(doc::Link const& I);
    void operator()(doc::ListItem const& I);
    void operator()(doc::Param const& I);
    void operator()(doc::Returns const& I);
    void operator()(doc::Text const& I);
    void operator()(doc::Styled const& I);
    void operator()(doc::TParam const& I);
    void operator()(doc::Reference const& I);
    void operator()(doc::Throws const& I);

    std::size_t measureLeftMargin(
        doc::List<doc::Text> const& list);
};

void
DocVisitor::
operator()(
    doc::Admonition const& I)
{
    std::string_view label;
    switch(I.admonish)
    {
    case doc::Admonish::note:
        label = "NOTE";
        break;
    case doc::Admonish::tip:
        label = "TIP";
        break;
    case doc::Admonish::important:
        label = "IMPORTANT";
        break;
    case doc::Admonish::caution:
        label = "CAUTION";
        break;
    case doc::Admonish::warning:
        label = "WARNING";
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    fmt::format_to(std::back_inserter(dest_), "[{}]\n", label);
    (*this)(static_cast<doc::Paragraph const&>(I));
}

void
DocVisitor::
operator()(
    doc::Code const& I)
{
    auto const leftMargin = measureLeftMargin(I.children);
    dest_ +=
        "[,cpp]\n"
        "----\n";
    for(auto const& it : RangeFor(I.children))
    {
        doc::visit(*it.value,
            [&]<class T>(T const& text)
            {
                if constexpr(std::is_same_v<T, doc::Text>)
                {
                    if(! text.string.empty())
                    {
                        std::string_view s = text.string;
                        s.remove_prefix(leftMargin);
                        dest_.append(s);
                    }
                    dest_.push_back('\n');
                }
                else
                {
                    MRDOCS_UNREACHABLE();
                }
            });
    }
    dest_ += "----\n";
}

void
DocVisitor::
operator()(
    doc::Heading const& I)
{
    fmt::format_to(ins_, "\n=== {}\n", I.string);
}

// Also handles doc::Brief
void
DocVisitor::
operator()(
    doc::Paragraph const& I)
{
    for(auto const& it : RangeFor(I.children))
    {
        auto const n = dest_.size();
        doc::visit(*it.value, *this);
        // detect empty text blocks
        if(! it.last && dest_.size() > n)
        {
            // wrap past 80 cols
            if(dest_.size() < 80)
                dest_.push_back(' ');
            else
                dest_.append("\n");
        }
    }
    dest_.push_back('\n');
    // dest_.push_back('\n');
}

void
DocVisitor::
operator()(
    doc::Link const& I)
{
    dest_.append("link:");
    dest_.append(I.href);
    dest_.push_back('[');
    dest_.append(I.string);
    dest_.push_back(']');
}

void
DocVisitor::
operator()(
    doc::ListItem const& I)
{
    dest_.append("\n* ");
    for(auto const& it : RangeFor(I.children))
    {
        auto const n = dest_.size();
        doc::visit(*it.value, *this);
        // detect empty text blocks
        if(! it.last && dest_.size() > n)
        {
            // wrap past 80 cols
            if(dest_.size() < 80)
                dest_.push_back(' ');
            else
                dest_.append("\n");
        }
    }
    dest_.push_back('\n');
}

void
DocVisitor::
operator()(doc::Param const& I)
{
}

void
DocVisitor::
operator()(doc::TParam const& I)
{
}

void
DocVisitor::
operator()(doc::Throws const& I)
{
}

void
DocVisitor::
operator()(doc::Returns const& I)
{
    (*this)(static_cast<doc::Paragraph const&>(I));
}

void
DocVisitor::
operator()(doc::Text const& I)
{
    // Asciidoc text must not have leading
    // else they can be rendered up as code.
    std::string_view s = trim(I.string);
    fmt::format_to(std::back_inserter(dest_), "pass:v,q[{}]", s);
}

void
DocVisitor::
operator()(doc::Styled const& I)
{
    // VFALCO We need to apply Asciidoc escaping
    // depending on the contents of the string.
    std::string_view s = trim(I.string);
    switch(I.style)
    {
    case doc::Style::none:
        dest_.append(s);
        break;
    case doc::Style::bold:
        fmt::format_to(std::back_inserter(dest_), "*{}*", s);
        break;
    case doc::Style::mono:
        fmt::format_to(std::back_inserter(dest_), "`{}`", s);
        break;
    case doc::Style::italic:
        fmt::format_to(std::back_inserter(dest_), "_{}_", s);
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
DocVisitor::
operator()(doc::Reference const& I)
{
    //dest_ += I.string;
    if(I.id == SymbolID::invalid)
        return (*this)(static_cast<const doc::Text&>(I));
    fmt::format_to(std::back_inserter(dest_), "xref:{}[{}]",
        corpus_.getXref(corpus_->get(I.id)), I.string);
}

std::size_t
DocVisitor::
measureLeftMargin(
    doc::List<doc::Text> const& list)
{
    if(list.empty())
        return 0;
    std::size_t n = std::size_t(-1);
    for(auto& text : list)
    {
        if(trim(text->string).empty())
            continue;
        auto const space =
            text->string.size() - ltrim(text->string).size();
        if( n > space)
            n = space;
    }
    return n;
}

static
dom::Value
domCreate(
    const doc::Param& I,
    const AdocCorpus& corpus)
{
    dom::Object::storage_type entries = {
        { "name", I.name }
    };
    std::string s;
    DocVisitor visitor(corpus, s);
    visitor(static_cast<const doc::Paragraph&>(I));
    if(! s.empty())
        entries.emplace_back(
            "description", std::move(s));
    return dom::Object(std::move(entries));
}

static
dom::Value
domCreate(
    const doc::TParam& I,
    const AdocCorpus& corpus)
{
    dom::Object::storage_type entries = {
        { "name", I.name }
    };
    std::string s;
    DocVisitor visitor(corpus, s);
    visitor(static_cast<const doc::Paragraph&>(I));
    if(! s.empty())
        entries.emplace_back(
            "description", std::move(s));
    return dom::Object(std::move(entries));
}

static
dom::Value
domCreate(
    const doc::Throws& I,
    const AdocCorpus& corpus)
{
    dom::Object::storage_type entries = {
        { "exception", I.exception }
    };
    std::string s;
    DocVisitor visitor(corpus, s);
    visitor(static_cast<const doc::Paragraph&>(I));
    if(! s.empty())
        entries.emplace_back(
            "description", std::move(s));
    return dom::Object(std::move(entries));
}

//------------------------------------------------

class DomJavadoc : public dom::LazyObjectImpl
{
    const AdocCorpus& corpus_;
    Javadoc const& jd_;

public:
    DomJavadoc(
        const AdocCorpus& corpus,
        Javadoc const& jd) noexcept
        : corpus_(corpus)
        , jd_(jd)
    {
    }

    template<std::derived_from<doc::Node> T>
    void
    maybeEmplace(
        storage_type& list,
        std::string_view key,
        T const& I) const
    {
        std::string s;
        DocVisitor visitor(corpus_, s);
        doc::visit(I, visitor);
        if(! s.empty())
            list.emplace_back(key, std::move(s));
    };

    template<class T>
    void
    maybeEmplace(
        storage_type& list,
        std::string_view key,
        std::vector<T const*> const& nodes) const
    {
        std::string s;
        DocVisitor visitor(corpus_, s);
        for(auto const& t : nodes)
            doc::visit(*t, visitor);
        if(! s.empty())
        {
            list.emplace_back(key, std::move(s));
        }
    };

    template<class T>
    void
    maybeEmplaceArray(
        storage_type& list,
        std::string_view key,
        std::vector<T const*> const& nodes) const
    {
        dom::Array::storage_type elements;
        elements.reserve(nodes.size());
        for(auto const& elem : nodes)
        {
            if(! elem)
                continue;
            elements.emplace_back(
                domCreate(*elem, corpus_));
        }
        if(elements.empty())
            return;
        list.emplace_back(key, dom::newArray<
            dom::DefaultArrayImpl>(std::move(elements)));
    };

    dom::Object
    construct() const override
    {
        storage_type list;
        list.reserve(2);

        auto ov = jd_.makeOverview(*corpus_);

        // brief
        if(ov.brief)
            maybeEmplace(list, "brief", *ov.brief);
        maybeEmplace(list, "description", ov.blocks);
        if(ov.returns)
            maybeEmplace(list, "returns", *ov.returns);
        maybeEmplaceArray(list, "params", ov.params);
        maybeEmplaceArray(list, "tparams", ov.tparams);
        maybeEmplaceArray(list, "exceptions", ov.exceptions);

        return dom::Object(std::move(list));
    }
};

} // (anon)

dom::Object
AdocCorpus::
construct(Info const& I) const
{
    // wraps a DomInfo with a lazy object which
    // adds additional properties to the wrapped
    // object once constructed.
    struct AdocInfo :
        public dom::LazyObjectImpl
    {
        Info const& I_;
        AdocCorpus const& adocCorpus_;

    public:
        AdocInfo(
            Info const& I,
            AdocCorpus const& adocCorpus) noexcept
            : I_(I)
            , adocCorpus_(adocCorpus)
        {
        }

        dom::Object construct() const override
        {
            auto obj = adocCorpus_.DomCorpus::construct(I_);
            obj.set("ref", adocCorpus_.getXref(I_));
            return obj;
        }
    };
    return dom::newObject<AdocInfo>(I, *this);
}

std::string
AdocCorpus::
getXref(Info const& I) const
{
    bool multipage = getCorpus().config->multiPage;
    // use '/' as the seperator for multi-page, and '-' for single-page
    std::string xref = names_.getQualified(
        I.id, multipage ? '/' : '-');
    // add the file extension if in multipage mode
    if(multipage)
        xref.append(".adoc");
    return xref;
}

std::string
AdocCorpus::
getXref(OverloadSet const& os) const
{
    bool multipage = getCorpus().config->multiPage;
    // use '/' as the seperator for multi-page, and '-' for single-page
    std::string xref = names_.getQualified(
        os, multipage ? '/' : '-');
    // add the file extension if in multipage mode
    if(multipage)
        xref.append(".adoc");
    return xref;
}

dom::Value
AdocCorpus::
getJavadoc(
    Javadoc const& jd) const
{
    return dom::newObject<DomJavadoc>(*this, jd);
}

dom::Object
AdocCorpus::
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
    // return dom::newObject<AdocOverloads>(os, *this);
}

} // adoc
} // mrdocs
} // clang
