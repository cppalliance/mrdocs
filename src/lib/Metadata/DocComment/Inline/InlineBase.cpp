//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/String.hpp>

namespace mrdocs::doc {

void
getAsPlainText(doc::Inline const& in, std::string& dst)
{
    if (auto const* text = dynamic_cast<doc::TextInline const*>(&in))
    {
        dst += text->literal;
        return;
    }
    else if (in.isSoftBreak())
    {
        dst += ' ';
        return;
    }
    else if (in.isLineBreak())
    {
        dst += '\n';
        return;
    }
    else if (auto const* container = dynamic_cast<doc::InlineContainer const*>(&in))
    {
        getAsPlainText(*container, dst);
        return;
    }
}

std::strong_ordering
InlineContainer::
operator<=>(InlineContainer const& rhs) const
{
    return this->children <=> rhs.children;
}

InlineContainer::
InlineContainer(std::string_view text)
{
    operator=(text);
}

InlineContainer::
InlineContainer(std::string const& text)
    : InlineContainer(std::string_view(text))
{}


InlineContainer::
InlineContainer(std::string&& text)
    : InlineContainer(std::string_view(text))
{}


InlineContainer&
InlineContainer::
operator=(std::string_view text)
{
    this->children.clear();
    return append(text);
}


InlineContainer&
InlineContainer::
append(std::string_view text)
{
    if (!text.empty())
    {
        this->children.push_back(Polymorphic<Inline>(std::in_place_type<TextInline>, text));
    }
    return *this;
}

void
ltrim(InlineContainer& inlines)
{
    for (auto it = inlines.children.begin(); it != inlines.children.end();)
    {
        auto& child = *it;
        visit(*child, []<class InlineTy>(InlineTy& N) {
            if constexpr (std::derived_from<InlineTy, InlineContainer>)
            {
                ltrim(N.asInlineContainer());
            }
            if constexpr (requires { { N.literal } -> std::same_as<std::string&>; })
            {
                std::string& text = N.literal;
                text = mrdocs::ltrim(text);
            }
        });
        if (!isEmpty(child))
        {
            break;
        }
        it = inlines.children.erase(it);
    }
}

void
rtrim(InlineContainer& inlines)
{
    for (auto it = inlines.children.rbegin(); it != inlines.children.rend();)
    {
        auto& child = *it;
        visit(*child, []<class InlineTy>(InlineTy& N) {
            if constexpr (std::derived_from<InlineTy, InlineContainer>)
            {
                rtrim(N.asInlineContainer());
            }
            if constexpr (requires { { N.literal } -> std::same_as<std::string&>; })
            {
                std::string& text = N.literal;
                text = mrdocs::rtrim(text);
            }
        });
        if (!isEmpty(child))
        {
            break;
        }
        it = decltype(it){ inlines.children.erase(std::next(it).base()) };
    }
}

void
getAsPlainText(doc::InlineContainer const& in, std::string& dst)
{
    for (auto const& child : in.children)
    {
        getAsPlainText(*child, dst);
    }
}

} // mrdocs::doc