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

#include "DocVisitor.hpp"
#include "lib/Support/Radix.hpp"
#include <iterator>
#include <fmt/format.h>
#include <llvm/Support/raw_ostream.h>
#include <mrdocs/Support/RangeFor.hpp>
#include <mrdocs/Support/String.hpp>

namespace clang {
namespace mrdocs {
namespace html {

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
    fmt::format_to(std::back_inserter(dest_),
        "<div>\n<h4>{}</h4>\n", label);
    (*this)(static_cast<doc::Paragraph const&>(I));
    dest_.append("</div>\n");
}

void
DocVisitor::
operator()(
    doc::Code const& I)
{
    auto const leftMargin = measureLeftMargin(I.children);
    dest_.append("<code>\n");
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
    dest_.append("</code>\n");
}

void
DocVisitor::
operator()(
    doc::Heading const& I)
{
    fmt::format_to(ins_, "<h3>{}</h3>\n", I.string);
}

// Also handles doc::Brief
void
DocVisitor::
operator()(
    doc::Paragraph const& I)
{
    dest_.append("<p>");
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
                dest_.push_back('\n');
        }
    }
    dest_.append("</p>\n\n");
}

void
DocVisitor::
operator()(
    doc::Link const& I)
{
    dest_.append("<a href=\"");
    dest_.append(I.href);
    dest_.append("\">");
    dest_.append(I.string);
    dest_.append("</a>");
}

void
DocVisitor::
operator()(
    doc::ListItem const& I)
{
    dest_.append("<li>");
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
    dest_.append("</li>\n");
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
    std::string_view s = trim(I.string);
    fmt::format_to(std::back_inserter(dest_), "<span>{}</span>", s);
}

void
DocVisitor::
operator()(doc::Styled const& I)
{
    std::string_view s = trim(I.string);
    switch(I.style)
    {
    case doc::Style::none:
        dest_.append(s);
        break;
    case doc::Style::bold:
        fmt::format_to(std::back_inserter(dest_), "<b>{}</b>", s);
        break;
    case doc::Style::mono:
        fmt::format_to(std::back_inserter(dest_), "<code>{}</code>", s);
        break;
    case doc::Style::italic:
        fmt::format_to(std::back_inserter(dest_), "<i>{}</i>", s);
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
DocVisitor::
operator()(doc::TParam const& I)
{
    //dest_ += I.string;
}

void
DocVisitor::
operator()(doc::Reference const& I)
{
    if(I.id == SymbolID::invalid)
        return (*this)(static_cast<const doc::Text&>(I));
    fmt::format_to(std::back_inserter(dest_), "<a href=\"{}\">{}</a>",
        corpus_.getXref(corpus_->get(I.id)), I.string);
}

void
DocVisitor::
operator()(doc::Throws const& I)
{
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

} // html
} // mrdocs
} // clang
