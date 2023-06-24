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

#ifndef MRDOX_TOOL_HTML_DOCVISITOR_HPP
#define MRDOX_TOOL_HTML_DOCVISITOR_HPP

#include "HTMLTag.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Support/String.hpp>
#include <fmt/format.h>

namespace clang {
namespace mrdox {
namespace html {

struct DocVisitor
{
    void operator()(doc::List<doc::Block> const& list, HTMLTagWriter& tag);

    void operator()(doc::Admonition const& I, HTMLTagWriter& tag);
    void operator()(doc::Code const& I, HTMLTagWriter& tag);
    void operator()(doc::Heading const& I, HTMLTagWriter& tag);
    void operator()(doc::Paragraph const& I, HTMLTagWriter& tag);
    void operator()(doc::Link const& I, HTMLTagWriter& tag);
    void operator()(doc::ListItem const& I, HTMLTagWriter& tag);
    void operator()(doc::Param const& I, HTMLTagWriter& tag);
    void operator()(doc::Returns const& I, HTMLTagWriter& tag);
    void operator()(doc::Text const& I, HTMLTagWriter& tag);
    void operator()(doc::Styled const& I, HTMLTagWriter& tag);
    void operator()(doc::TParam const& I, HTMLTagWriter& tag);

    std::size_t measureLeftMargin(
        doc::List<doc::Text> const& list);
};

void
DocVisitor::
operator()(
    doc::List<doc::Block> const& list,
    HTMLTagWriter& tag)
{
    for(auto const& block : list)
        doc::visit(*block, *this, tag);
}

void
DocVisitor::
operator()(
    doc::Admonition const& I,
    HTMLTagWriter& tag)
{

}

void
DocVisitor::
operator()(
    doc::Code const& I,
    HTMLTagWriter& tag)
{
    auto const leftMargin = measureLeftMargin(I.children);
    HTMLTagWriter code({
        .Name = "div",
        .Class = "jd-code"
    });
    for(const auto& c : I.children)
    {
        doc::visit(*c,
            [&]<class T>(T const& text)
            {
                if constexpr(std::is_same_v<T, doc::Text>)
                {
                    if(! text.string.empty())
                    {
                        std::string_view s = text.string;
                        s.remove_prefix(leftMargin);
                        code.write(s);
                    }
                    code.write("\n");
                }
                else
                {
                    MRDOX_UNREACHABLE();
                }
            });
    }
    tag.write(code);
}

void
DocVisitor::
operator()(
    doc::Heading const& I,
    HTMLTagWriter& tag)
{
    tag.write({
        .Name = "span",
        .Class = "jd-heading",
        .Content = trim(I.string)
    });
}

void
DocVisitor::
operator()(
    doc::Paragraph const& I,
    HTMLTagWriter& tag)
{
    HTMLTagWriter para({
        .Name = "p",
        .Class = "jd-paragraph",
    });
    if(! I.children.empty())
    {
        auto first = I.children.begin();
        doc::visit(**first++, *this, para);
        for(const auto last = I.children.end(); first != last;)
        {
            para.write(" ");
            doc::visit(**first++, *this, para);
        }
    }
    tag.write(para);
}

void
DocVisitor::
operator()(
    doc::Link const& I,
    HTMLTagWriter& tag)
{
    tag.write({
        .Name = "a",
        .Class = "jd-link",
        .Attrs = {{"href", I.href}},
        .Content = I.string
    });
}

void
DocVisitor::
operator()(
    doc::ListItem const& I,
    HTMLTagWriter& tag)
{
    HTMLTagWriter list({
        .Name = "ul",
        .Class = "jd-list"
    });
    for(const auto& c : I.children)
    {
        HTMLTagWriter item({
            .Name = "li",
            .Class = "jd-list-item"
        });
        doc::visit(*c, *this, item);
        if(item.has_content())
            list.write(item);
    }
    tag.write(list);
}

void
DocVisitor::
operator()(
    doc::Param const& I,
    HTMLTagWriter& tag)
{
}

void
DocVisitor::
operator()(
    doc::Returns const& I,
    HTMLTagWriter& tag)
{
}

void
DocVisitor::
operator()(
    doc::Text const& I,
    HTMLTagWriter& tag)
{
    tag.write(trim(I.string));
}

void
DocVisitor::
operator()(
    doc::Styled const& I,
    HTMLTagWriter& tag)
{
    std::string_view style_class;
    switch(I.style)
    {
    case doc::Style::none:
        style_class = "jd-style-none";
        break;
    case doc::Style::bold:
        style_class = "jd-style-bold";
        break;
    case doc::Style::mono:
        style_class = "jd-style-mono";
        break;
    case doc::Style::italic:
        style_class = "jd-style-italic";
        break;
    default:
        MRDOX_UNREACHABLE();
    }
    tag.write({
        .Name = "span",
        .Class = style_class,
        .Content = trim(I.string)
    });
}

void
DocVisitor::
operator()(
    doc::TParam const& I,
    HTMLTagWriter& tag)
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
} // mrdox
} // clang

#endif
