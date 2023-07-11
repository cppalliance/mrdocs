//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "AdocCorpus.hpp"
#include <mrdox/Support/RangeFor.hpp>
#include <mrdox/Support/String.hpp>
#include <fmt/format.h>
#include <iterator>

#include <llvm/Support/raw_ostream.h>

namespace clang {
namespace mrdox {
namespace adoc {

namespace {

class DocVisitor
{
    std::string& dest_;
    std::back_insert_iterator<std::string> ins_;

public:
    explicit DocVisitor(std::string& dest) noexcept;

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

    std::size_t measureLeftMargin(
        doc::List<doc::Text> const& list);
};

DocVisitor::
DocVisitor(
    std::string& dest) noexcept
    : dest_(dest)
    , ins_(std::back_inserter(dest_))
{
}

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
        MRDOX_UNREACHABLE();
    }
    dest_ += fmt::format("[{}]\n", label);
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
                    MRDOX_UNREACHABLE();
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
    fmt::format_to(ins_, "=== {}\n", I.string);
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
    dest_.push_back('\n');
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
    dest_.append("* ");
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
    //dest_ += I.string;
}

void
DocVisitor::
operator()(doc::Returns const& I)
{
    //dest_ += I.string;
}

void
DocVisitor::
operator()(doc::Text const& I)
{
    // Asciidoc text must not have leading 
    // else they can be rendered up as code.
    std::string_view s = trim(I.string);
    dest_.append(s);
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
        MRDOX_UNREACHABLE();
    }
}

void
DocVisitor::
operator()(doc::TParam const& I)
{
    //dest_ += I.string;
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

//------------------------------------------------

class DomJavadoc : public dom::LazyObjectImpl
{
    Javadoc const& jd_;

public:
    DomJavadoc(
        Javadoc const& jd) noexcept
        : jd_(jd)
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
        DocVisitor visitor(s);
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
        DocVisitor visitor(s);
        for(auto const& t : nodes)
            doc::visit(*t, visitor);
        if(! s.empty())
        {
            list.emplace_back(key, std::move(s));
        }
    };

    dom::Object
    construct() const override
    {
        storage_type list;
        list.reserve(2);

        auto ov = jd_.makeOverview();

        // brief
        if(ov.brief)
            maybeEmplace(list, "brief", *ov.brief);
        maybeEmplace(list, "description", ov.blocks);
        if(ov.returns)
            maybeEmplace(list, "returns", *ov.returns);
        maybeEmplace(list, "params", ov.params);
        maybeEmplace(list, "tparams", ov.tparams);

        return dom::Object(std::move(list));
    }
};

} // (anon)

dom::Value
AdocCorpus::
getJavadoc(
    Javadoc const& jd) const
{
    return dom::newObject<DomJavadoc>(jd);
}

} // adoc
} // mrdox
} // clang
