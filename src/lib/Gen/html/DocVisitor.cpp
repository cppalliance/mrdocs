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
#include "mrdocs/Support/Handlebars.hpp"
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
    doc::Admonition const& I) const
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
    doc::Code const& I) const
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
    doc::Heading const& I) const
{
    fmt::format_to(ins_, "<h3>{}</h3>\n", I.string);
}

// Also handles doc::Brief
void
DocVisitor::
operator()(
    doc::Paragraph const& I) const
{
    if (I.children.empty())
    {
        return;
    }

    dest_.append("<p>");
    std::size_t i = 0;
    for (auto it = I.children.begin(); it != I.children.end(); ++it)
    {
        auto& child = *it;
        if (i == 0)
        {
            child->string = ltrim(child->string);
        }
        else if (auto prevIt = std::prev(it);
            !(*prevIt)->string.empty() && !child->string.empty())
        {
            char const pc = (*(prevIt))->string.back();
            char const cc = child->string.front();
            if (!std::isspace(pc) && !std::isspace(cc))
            {
                dest_.push_back(' ');
            }
        }
        if (i == I.children.size() - 1)
        {
            child->string = rtrim(child->string);
        }
        write(*child, *this);
        i = i + 1;
    }
    dest_.append("</p>\n");
}

void
DocVisitor::
operator()(
    doc::Brief const& I) const
{
    dest_.append("<span>");
    for(auto const& it : RangeFor(I.children))
    {
        auto const n = dest_.size();
        doc::visit(*it.value, *this);
        // detect empty text blocks
        if(! it.last && dest_.size() > n)
        {
            // wrap past 80 cols
            if (dest_.size() < 80)
            {
                dest_.push_back(' ');
            } else
            {
                dest_.push_back('\n');
            }
        }
    }
    dest_.append("</span>");
}

void
DocVisitor::
operator()(
    doc::Link const& I) const
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
    doc::ListItem const& I) const
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
            if (dest_.size() < 80)
            {
                dest_.push_back(' ');
            } else
            {
                dest_.append("\n");
            }
        }
    }
    dest_.append("</li>\n");
}

void
DocVisitor::
operator()(doc::Param const& I) const
{
    this->operator()(static_cast<doc::Paragraph const&>(I));
}

void
DocVisitor::
operator()(doc::Returns const& I) const
{
    (*this)(static_cast<doc::Paragraph const&>(I));
}

void
DocVisitor::
operator()(doc::Text const& I) const
{
    fmt::format_to(std::back_inserter(dest_), "<span>{}</span>", HTMLEscape(I.string));
}

void
DocVisitor::
operator()(doc::Styled const& I) const
{
    switch(I.style)
    {
    case doc::Style::none:
        dest_.append(I.string);
        break;
    case doc::Style::bold:
        fmt::format_to(std::back_inserter(dest_), "<b>{}</b>", HTMLEscape(I.string));
        break;
    case doc::Style::mono:
        fmt::format_to(std::back_inserter(dest_), "<code>{}</code>", HTMLEscape(I.string));
        break;
    case doc::Style::italic:
        fmt::format_to(std::back_inserter(dest_), "<i>{}</i>", HTMLEscape(I.string));
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
DocVisitor::
operator()(doc::TParam const& I) const
{
    this->operator()(static_cast<doc::Paragraph const&>(I));
}

void
DocVisitor::
operator()(doc::Reference const& I) const
{
    if (I.id == SymbolID::invalid)
    {
        return (*this)(static_cast<doc::Text const&>(I));
    }
    // AFREITAS: Unlike Adoc, we need relative URLs for HTML
    fmt::format_to(std::back_inserter(dest_), "<a href=\"{}\">{}</a>",
        corpus_.getURL(corpus_->get(I.id)), I.string);
}

void
DocVisitor::
operator()(doc::Throws const& I) const
{
    this->operator()(static_cast<doc::Paragraph const&>(I));
}

std::size_t
DocVisitor::
measureLeftMargin(
    doc::List<doc::Text> const& list)
{
    if(list.empty())
    {
        return 0;
    }
    auto n = static_cast<std::size_t>(-1);
    for(auto& text : list)
    {
        if (trim(text->string).empty())
        {
            continue;
        }
        auto const space =
            text->string.size() - ltrim(text->string).size();
        if (n > space)
        {
            n = space;
        }
    }
    return n;
}

} // html
} // mrdocs
} // clang
