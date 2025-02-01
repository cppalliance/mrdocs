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
#include "lib/Gen/adoc/AdocEscape.hpp"
#include "lib/Support/Radix.hpp"
#include <iterator>
#include <fmt/format.h>
#include <llvm/Support/raw_ostream.h>
#include <mrdocs/Support/RangeFor.hpp>
#include <mrdocs/Support/String.hpp>
#include <ranges>

namespace clang::mrdocs::adoc {

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
    fmt::format_to(std::back_inserter(dest_), "[{}]\n", label);
    (*this)(static_cast<doc::Paragraph const&>(I));
}

void
DocVisitor::
operator()(
    doc::Code const& I) const
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
                if constexpr(std::derived_from<T, doc::Text>)
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
    doc::Heading const& I) const
{
    fmt::format_to(ins_, "\n=== {}\n\n", AdocEscape(I.string));
}

void
DocVisitor::
operator()(
    doc::Paragraph const& I) const
{
    if (I.children.empty())
    {
        return;
    }

    std::size_t i = 0;
    for (auto it = I.children.begin(); it != I.children.end(); ++it)
    {
        auto& child = *it;
        if (i != 0 &&
            i != I.children.size() - 1)
        {
            write(*child, *this);
        }
        else
        {
            auto childCopy = *it;
            if (i == 0)
            {
                childCopy->string = ltrim(childCopy->string);
            }
            if (i == I.children.size() - 1)
            {
                childCopy->string = rtrim(childCopy->string);
            }
            write(*childCopy, *this);
        }
        i = i + 1;
    }

    dest_.push_back('\n');
    dest_.push_back('\n');
}

void
DocVisitor::
operator()(
    doc::Brief const& I) const
{
    std::span const children = I.children;
    if (children.empty())
    {
        return;
    }

    std::size_t i = 0;
    for (auto it = children.begin(); it != children.end(); ++it)
    {
        auto& child = *it;
        if (i != 0 &&
            i != children.size() - 1)
        {
            write(*child, *this);
        }
        else
        {
            auto childCopy = *it;
            if (i == 0)
            {
                childCopy->string = ltrim(childCopy->string);
            }
            if (i == children.size() - 1)
            {
                childCopy->string = rtrim(childCopy->string);
            }
            write(*childCopy, *this);
        }
        i = i + 1;
    }
}

void
DocVisitor::
operator()(
    doc::Link const& I) const
{
    dest_.append("link:");
    dest_.append(I.href);
    dest_.push_back('[');
    dest_.append(AdocEscape(I.string));
    dest_.push_back(']');
}

void
DocVisitor::
operator()(
    doc::ListItem const& I) const
{
    std::span children = I.children;
    if (children.empty())
    {
        return;
    }
    dest_.append("* ");
    bool non_empty = write(*children.front(), *this);
    for(auto const& child : children.subspan(1))
    {
        if (non_empty)
        {
            dest_.push_back('\n');
        }
        non_empty = write(*child, *this);
    }
    dest_.push_back('\n');
}

void
DocVisitor::
operator()(
    doc::UnorderedList const& I) const
{
    if (I.items.empty())
    {
        return;
    }
    for(auto const& child : I.items)
    {
        operator()(child);
    }
    dest_.push_back('\n');
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
    if (I.string.empty())
    {
        dest_.append("\n\n");
    } else
    {
        dest_.append(AdocEscape(I.string));
    }
}

void
DocVisitor::
operator()(doc::Styled const& I) const
{
    switch(I.style)
    {
    case doc::Style::none:
        dest_.append(AdocEscape(I.string));
        break;
    case doc::Style::bold:
        fmt::format_to(std::back_inserter(dest_), "*{}*", AdocEscape(I.string));
        break;
    case doc::Style::mono:
        fmt::format_to(std::back_inserter(dest_), "`{}`", AdocEscape(I.string));
        break;
    case doc::Style::italic:
        fmt::format_to(std::back_inserter(dest_), "_{}_", AdocEscape(I.string));
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
    dom::Object symbolObj = corpus_.construct(corpus_->get(I.id));
    if (symbolObj.exists("url"))
    {
        std::string url = symbolObj.get("url").getString().str();
        if (url.starts_with('/'))
        {
            url.erase(0, 1);
        }
        fmt::format_to(
            std::back_inserter(dest_),
            "xref:{}[{}]",
            url, AdocEscape(I.string));
        return;
    }
    fmt::format_to(
            std::back_inserter(dest_),
            "`{}`",
            AdocEscape(I.string));
}

void
DocVisitor::
operator()(doc::Throws const& I) const
{
    this->operator()(static_cast<doc::Paragraph const&>(I));
}

std::size_t
DocVisitor::
measureLeftMargin(std::vector<Polymorphic<doc::Text>> const& list)
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

} // clang::mrdocs::adoc
