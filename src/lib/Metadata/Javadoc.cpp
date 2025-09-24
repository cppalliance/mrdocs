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

#include <lib/Support/Debug.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Metadata/Javadoc/Text/Parts.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <format>

namespace clang {
namespace mrdocs {
namespace doc {

dom::String
toString(NodeKind kind) noexcept
{
    switch(kind)
    {
        case NodeKind::text:
            return "text";
        case NodeKind::admonition:
            return "admonition";
        case NodeKind::brief:
            return "brief";
        case NodeKind::code:
            return "code";
        case NodeKind::heading:
            return "heading";
        case NodeKind::link:
            return "link";
        case NodeKind::list_item:
            return "list_item";
        case NodeKind::unordered_list:
            return "unordered_list";
        case NodeKind::paragraph:
            return "paragraph";
        case NodeKind::param:
            return "param";
        case NodeKind::returns:
            return "returns";
        case NodeKind::styled:
            return "styled";
        case NodeKind::tparam:
            return "tparam";
        case NodeKind::reference:
            return "reference";
        case NodeKind::copy_details:
            return "copied";
        case NodeKind::throws:
            return "throws";
        case NodeKind::details:
            return "details";
        case NodeKind::see:
            return "see";
        case NodeKind::precondition:
            return "precondition";
        case NodeKind::postcondition:
            return "postcondition";
        default:
            MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(Style kind) noexcept
{
    switch(kind)
    {
    case Style::none:
        return "";
    case Style::mono:
        return "mono";
    case Style::bold:
        return "bold";
    case Style::italic:
        return "italic";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(Admonish kind) noexcept
{
    switch(kind)
    {
    case Admonish::none:
        return "";
    case Admonish::note:
        return "note";
    case Admonish::tip:
        return "tip";
    case Admonish::important:
        return "important";
    case Admonish::caution:
        return "caution";
    case Admonish::warning:
        return "warning";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(ParamDirection kind) noexcept
{
    switch(kind)
    {
    case ParamDirection::none:
        return "";
    case ParamDirection::in:
        return "in";
    case ParamDirection::out:
        return "out";
    case ParamDirection::inout:
        return "inout";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(Parts kind) noexcept
{
    switch(kind)
    {
    case Parts::all:
        return "all";
    case Parts::brief:
        return "brief";
    case Parts::description:
        return "description";
    default:
        MRDOCS_UNREACHABLE();
    }
}

Text&
Block::
emplace_back(
    Polymorphic<Text> text)
{
    MRDOCS_ASSERT(text->isText());
    return *children.emplace_back(std::move(text));
}

void
Block::
append(std::vector<Polymorphic<Node>>&& blocks)
{
    children.reserve(children.size() + blocks.size());
    for (auto&& block : blocks)
    {
        MRDOCS_ASSERT(block->isText());
        emplace_back(Polymorphic<Text>(std::in_place_type<Text>,
                                       dynamic_cast<Text &&>(*block)));
    }
}

void
Block::
append(std::vector<Polymorphic<Text>> const& otherChildren)
{
    children.insert(
        children.end(),
        otherChildren.begin(),
        otherChildren.end());
}


std::strong_ordering
operator<=>(Polymorphic<Text> const& lhs, Polymorphic<Text> const& rhs)
{
    if (!lhs.valueless_after_move() && !rhs.valueless_after_move())
    {
        if (lhs->Kind == rhs->Kind)
        {
            return visit(*lhs, detail::VisitCompareFn<Node>(*rhs));
        }
        return lhs->Kind <=> rhs->Kind;
    }
    if (lhs.valueless_after_move() && rhs.valueless_after_move())
    {
        return std::strong_ordering::equal;
    }
    return lhs.valueless_after_move() ?
               std::strong_ordering::less :
               std::strong_ordering::greater;
}

Paragraph&
Paragraph::
operator=(std::string_view str)
{
    this->children.clear();
    this->children.emplace_back(Polymorphic<doc::Text>(
        std::in_place_type<doc::Text>, std::string(str)));
    return *this;
}

} // doc

//------------------------------------------------

Javadoc::
Javadoc() noexcept = default;

Javadoc::
Javadoc(
    std::vector<Polymorphic<doc::Block>> blocks)
    : blocks(std::move(blocks))
{
}

bool
Javadoc::
operator==(Javadoc const& other) const noexcept
{
    return std::ranges::equal(blocks, other.blocks,
        [](auto const& a, auto const& b)
        {
            return a->equals(static_cast<doc::Node const&>(*b));
        }) &&
        returns == other.returns &&
        brief == other.brief &&
        params == other.params &&
        tparams == other.tparams &&
        exceptions == other.exceptions &&
        sees == other.sees &&
        related == other.related &&
        preconditions == other.preconditions &&
        postconditions == other.postconditions;
}

bool
Javadoc::
operator!=(
    Javadoc const& other) const noexcept
{
    return !(*this == other);
}

std::string
Javadoc::
emplace_back(
    Polymorphic<doc::Block> block)
{
    MRDOCS_ASSERT(block->isBlock());

    std::string result;
    switch(block->Kind)
    {
    case doc::NodeKind::param:
    {
        // check for duplicate parameter name
        auto t = dynamic_cast<doc::Param const*>(block.operator->());
        for(auto const& q : blocks)
        {
            if(q->Kind == doc::NodeKind::param)
            {
                auto u = dynamic_cast<doc::Param const*>(q.operator->());
                if(u->name == t->name)
                {
                  result = std::format("duplicate param {}", t->name);
                  break;
                }
            }
        }
        break;
    }
    case doc::NodeKind::tparam:
    {
        // check for duplicate template parameter name
        auto t = dynamic_cast<doc::TParam const*>(block.operator->());
        for(auto const& q : blocks)
        {
            if(q->Kind == doc::NodeKind::tparam)
            {
                auto u = dynamic_cast<doc::TParam const*>(q.operator->());
                if(u->name == t->name)
                {
                  result = std::format("duplicate tparam {}", t->name);
                  break;
                }
            }
        }
        break;
    }
    default:
        break;
    }

    blocks.emplace_back(std::move(block));
    return result;
}

void
Javadoc::
append(
    Javadoc&& other)
{
    using std::ranges::find;
    using std::ranges::copy_if;
    using std::views::transform;

    // blocks
    for (auto&& block: other.blocks)
    {
        emplace_back(std::move(block));
    }
    // returns
    copy_if(other.returns, std::back_inserter(returns),
        [&](auto const& r)
        {
            return find(returns, r) == returns.end();
        });
    // params
    copy_if(other.params, std::back_inserter(params),
        [&](auto const& p)
        {
            auto namesAndDirection = transform(params, [](auto const& q)
            {
                return std::make_pair(std::string_view(q.name), q.direction);
            });
            auto el = std::make_pair(std::string_view(p.name), p.direction);
            return find(namesAndDirection, el) == namesAndDirection.end();
        });
    // tparams
    copy_if(other.tparams, std::back_inserter(tparams),
        [&](auto const& p)
        {
            auto names = transform(tparams, &doc::TParam::name);
            return find(names, p.name) == names.end();
        });
    // exceptions
    copy_if(other.exceptions, std::back_inserter(exceptions),
        [&](auto const& e)
        {
            // e.exception.string
            auto exceptionRefs = transform(exceptions, &doc::Throws::exception);
            auto exceptionStrs = transform(exceptionRefs, &doc::Reference::string);
            return find(exceptionStrs, e) == exceptionStrs.end();
        });
    // sees
    copy_if(other.sees, std::back_inserter(sees),
        [&](auto const& s)
        {
            return find(sees, s) == sees.end();
        });
    // preconditions
    copy_if(other.preconditions, std::back_inserter(preconditions),
        [&](auto const& p)
        {
            return find(preconditions, p) == preconditions.end();
        });
    // postconditions
    copy_if(other.postconditions, std::back_inserter(postconditions),
        [&](auto const& p)
        {
            return find(postconditions, p) == postconditions.end();
        });
}

void
Javadoc::
append(std::vector<Polymorphic<doc::Node>>&& blocks)
{
    blocks.reserve(blocks.size() + blocks.size());
    for(auto&& blockAsNode : blocks)
    {
        if (auto *Block = dynamic_cast<doc::Block *>(&*blockAsNode))
        {
            emplace_back(std::move(*Block));
        }
        else
        {
            Polymorphic<doc::Node> tmp = std::move(blockAsNode);
        }
    }
}

} // mrdocs
} // clang
