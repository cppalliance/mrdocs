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

dom::String
toString(
    doc::Style style) noexcept
{
    switch(style)
    {
    case doc::Style::bold:   return "bold";
    case doc::Style::mono:   return "mono";
    case doc::Style::italic: return "italic";

    // should never get here
    case doc::Style::none: return "";

    default:
        // unknown style
        MRDOCS_UNREACHABLE();
    }
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

/** Return the Javadoc as a @ref dom::Value.
 */
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Javadoc const& doc,
    DomCorpus const* domCorpus)
{
    v = domCorpus->getJavadoc(doc);
}

} // mrdocs
} // clang
