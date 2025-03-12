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

#include "lib/Support/Debug.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

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
        case NodeKind::copied:
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
        emplace_back(DynamicCast<Text>(std::move(block)));
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
    if (lhs && rhs)
    {
        if (lhs->Kind == rhs->Kind)
        {
            return visit(*lhs, detail::VisitCompareFn<Node>(*rhs));
        }
        return lhs->Kind <=> rhs->Kind;
    }
    if (!lhs && !rhs)
    {
        return std::strong_ordering::equal;
    }
    return !lhs ? std::strong_ordering::less
            : std::strong_ordering::greater;
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

doc::Paragraph const*
Javadoc::
getBrief(Corpus const& corpus) const noexcept
{
    // Brief from a @brief tag
    doc::Block const* brief = nullptr;
    // The first paragraph promoted to brief
    doc::Block const* promoted_brief = nullptr;
    // A brief copied from another symbol
    doc::Block const* copied_brief = nullptr;
    for(auto const& block : blocks)
    {
        if (!brief && block->Kind == doc::NodeKind::brief)
        {
            brief = block.operator->();
        }
        if (!promoted_brief && block->Kind == doc::NodeKind::paragraph)
        {
            promoted_brief = block.operator->();
        }

        // if we already have an explicit/copied brief,
        // don't check for additional copied briefs
        if (brief || copied_brief)
        {
            continue;
        }

        // Look for a @copydoc command
        for (auto const& text : block->children)
        {
            if (text->Kind != doc::NodeKind::copied)
            {
                continue;
            }
            if (auto const* copied = dynamic_cast<doc::Copied const*>(text.operator->());
                copied->id &&
                (copied->parts == doc::Parts::all ||
                copied->parts == doc::Parts::brief))
            {
                // Look for the symbol to copy from
                if (auto& jd = corpus.get(copied->id).javadoc)
                {
                    copied_brief = jd->getBrief(corpus);
                }
            }
        }
    }
    // An explicit brief superceeds a copied brief
    if (!brief)
    {
        // No explicit brief: use copied brief
        brief = copied_brief;
    }
    // A copied brief superceeds a promoted brief
    if (!brief)
    {
        // No copied brief: use promoted brief
        brief = promoted_brief;
    }
    return dynamic_cast<doc::Paragraph const*>(brief);
}

std::vector<Polymorphic<doc::Block>> const&
Javadoc::
getDescription(Corpus const& corpus) const noexcept
{
    for (auto const& block : blocks)
    {
        for(auto const& text : block->children)
        {
            if (!IsA<doc::Copied>(text))
            {
                continue;
            }
            if (auto const* copied = dynamic_cast<doc::Copied const*>(text.operator->());
                copied->id &&
                (copied->parts == doc::Parts::all ||
                 copied->parts == doc::Parts::description))
            {
                if (auto& jd = corpus.get(copied->id).javadoc)
                {
                    return jd->getDescription(corpus);
                }
            }
        }
    }
    return blocks;
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
                    result = fmt::format(
                        "duplicate param {}", t->name);
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
                    result = fmt::format(
                        "duplicate tparam {}", t->name);
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
        if (IsA<doc::Block>(blockAsNode))
        {
            emplace_back(DynamicCast<doc::Block>(std::move(blockAsNode)));
        }
        else
        {
            blockAsNode = {};
        }
    }
}

} // mrdocs
} // clang
