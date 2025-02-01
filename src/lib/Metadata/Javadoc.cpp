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
    : blocks_(std::move(blocks))
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
    for(auto const& block : blocks_)
    {
        if (!brief && block->kind == doc::Kind::brief)
        {
            brief = block.operator->();
        }
        if (!promoted_brief && block->kind == doc::Kind::paragraph)
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
            if (text->kind != doc::Kind::copied)
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
    return static_cast<const doc::Paragraph*>(brief);
}

std::vector<Polymorphic<doc::Block>> const&
Javadoc::
getDescription(Corpus const& corpus) const noexcept
{
    for (auto const& block : blocks_)
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
    return blocks_;
}

bool
Javadoc::
operator==(Javadoc const& other) const noexcept
{
    return std::ranges::equal(blocks_, other.blocks_,
        [](auto const& a, auto const& b)
        {
            return a->equals(static_cast<const doc::Node&>(*b));
        });
}

bool
Javadoc::
operator!=(
    Javadoc const& other) const noexcept
{
    return !(*this == other);
}

doc::Overview
Javadoc::
makeOverview(
    const Corpus& corpus) const
{
    doc::Overview ov;
    doc::Paragraph const* briefP = getBrief(corpus);
    if (briefP)
    {
        // Brief can be a brief or paragraph (if it was promoted)
        // Ensure it's always a brief so that they are all
        // rendered the same way.
        ov.brief = std::make_shared<doc::Brief>();
        ov.brief->append(briefP->children);
    }

    const auto& list = getDescription(corpus);
    // VFALCO dupes should already be reported as
    // warnings or errors by now so we don't have
    // to care about it here.

    for(auto it = list.begin();
        it != list.end(); ++it)
    {
        MRDOCS_ASSERT(*it);
        switch((*it)->kind)
        {
        case doc::Kind::brief:
            break;
        case doc::Kind::returns:
            ov.returns = dynamic_cast<
                doc::Returns const*>(it->operator->());
            break;
        case doc::Kind::param:
            ov.params.push_back(dynamic_cast<
                doc::Param const*>(it->operator->()));
            break;
        case doc::Kind::tparam:
            ov.tparams.push_back(dynamic_cast<
                doc::TParam const*>(it->operator->()));
            break;
        case doc::Kind::throws:
            ov.exceptions.push_back(dynamic_cast<
                doc::Throws const*>(it->operator->()));
            break;
        case doc::Kind::see:
            ov.sees.push_back(dynamic_cast<
                doc::See const*>(it->operator->()));
            break;
        case doc::Kind::precondition:
            ov.preconditions.push_back(dynamic_cast<
                doc::Precondition const*>(it->operator->()));
            break;
        case doc::Kind::postcondition:
            ov.postconditions.push_back(dynamic_cast<
                doc::Postcondition const*>(it->operator->()));
            break;
        default:
            if (briefP == it->operator->())
            {
                break;
            }
            ov.blocks.push_back(it->operator->());
        }
    }

    return ov;
}

std::string
Javadoc::
emplace_back(
    Polymorphic<doc::Block> block)
{
    MRDOCS_ASSERT(block->isBlock());

    std::string result;
    switch(block->kind)
    {
    case doc::Kind::param:
    {
        // check for duplicate parameter name
        auto t = dynamic_cast<doc::Param const*>(block.operator->());
        for(auto const& q : blocks_)
        {
            if(q->kind == doc::Kind::param)
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
    case doc::Kind::tparam:
    {
        // check for duplicate template parameter name
        auto t = dynamic_cast<doc::TParam const*>(block.operator->());
        for(auto const& q : blocks_)
        {
            if(q->kind == doc::Kind::tparam)
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

    blocks_.emplace_back(std::move(block));
    return result;
}

void
Javadoc::
append(
    Javadoc&& other)
{
    // VFALCO What about the returned strings,
    // for warnings and errors?
    for(auto&& block : other.blocks_)
        emplace_back(std::move(block));
}

void
Javadoc::
append(std::vector<Polymorphic<doc::Node>>&& blocks)
{
    blocks_.reserve(blocks_.size() + blocks.size());
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
