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
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdocs {

namespace doc {

Text&
Block::
emplace_back(
    std::unique_ptr<Text> text)
{
    MRDOCS_ASSERT(text->isText());
    return *children.emplace_back(std::move(text));
}

void
Block::
append(List<Node>&& blocks)
{
    children.reserve(children.size() + blocks.size());
    for(auto&& block : blocks)
    {
        MRDOCS_ASSERT(block->isText());
        emplace_back(std::unique_ptr<Text>(
            static_cast<Text*>(block.release())));
    }
}

#if 0
static
Overview
makeOverview(
    Block* brief,
    List<Block> const& list)
{
    doc::Overview ov;

    // VFALCO dupes should already be reported as
    // warnings or errors by now so we don't have
    // to care about it here.

    for(auto it = list.begin();
        it != list.end(); ++it)
    {
        switch((*it)->kind)
        {
        case Kind::brief:
            // ov.brief = static_cast<
            //     Paragraph const*>(it->get());
            break;
        case Kind::returns:
            ov.returns = static_cast<
                Returns const*>(it->get());
            break;
        case Kind::param:
            ov.params.push_back(static_cast<
                Param const*>(it->get()));
            break;
        case Kind::tparam:
            ov.tparams.push_back(static_cast<
                TParam const*>(it->get()));
            break;
            #if 0
        case Kind::paragraph:
            if(! ov.brief)
                ov.brief = static_cast<
                    Paragraph const*>(it->get());
            else
                ov.blocks.push_back(it->get());
            break;
            #endif
        default:
            if(ov.brief == it->get())
                break;
            ov.blocks.push_back(it->get());
        }
    }

    return ov;
}
#endif

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

template<typename T, typename U>
static
auto
move_to(
    doc::List<T>& dst,
    doc::List<U>& src,
    typename doc::List<U>::iterator it)
{
    auto elem = std::move(*it);
    it = src.erase(it);
    dst.emplace(dst.end(),
        static_cast<T*>(elem.release()));
    return it;
}

//------------------------------------------------

Javadoc::
Javadoc() noexcept = default;

Javadoc::
Javadoc(
    doc::List<doc::Block> blocks)
    : blocks_(std::move(blocks))
{
}

doc::Paragraph const*
Javadoc::
getBrief(Corpus const& corpus) const noexcept
{
    const doc::Block* brief = nullptr;
    const doc::Block* promoted_brief = nullptr;
    const doc::Block* copied_brief = nullptr;
    for(auto const& block : blocks_)
    {
        if(! brief &&
            block->kind == doc::Kind::brief)
            brief = block.get();
        if(! promoted_brief &&
            block->kind == doc::Kind::paragraph)
            promoted_brief = block.get();

        // if we already have an explicit/copied brief,
        // don't check for additional copied briefs
        if(brief || copied_brief)
            continue;

        for(auto const& text : block->children)
        {
            if(text->kind != doc::Kind::copied)
                continue;
            auto* copy = static_cast<doc::Copied*>(text.get());
            if(copy->id &&
                (copy->parts == doc::Parts::all ||
                copy->parts == doc::Parts::brief))
            {
                if(auto& jd = corpus.get(copy->id).javadoc)
                    copied_brief = jd->getBrief(corpus);
            }
        }
    }
    // an explicit brief superceeds a copied brief
    if(! brief)
        brief = copied_brief;
    // a copied brief superceeds a promoted brief
    if(! brief)
        brief = promoted_brief;
    return static_cast<const doc::Paragraph*>(brief);
}

doc::List<doc::Block> const&
Javadoc::
getDescription(Corpus const& corpus) const noexcept
{
    for(auto const& block : blocks_)
    {
        for(auto const& text : block->children)
        {
            if(text->kind != doc::Kind::copied)
                continue;
            auto* copy = static_cast<doc::Copied*>(text.get());
            if(copy->id &&
                (copy->parts == doc::Parts::all ||
                copy->parts == doc::Parts::description))
            {
                if(auto& jd = corpus.get(copy->id).javadoc)
                    return jd->getDescription(corpus);
            }
        }
    }
    return blocks_;
}

bool
Javadoc::
operator==(
    Javadoc const& other) const noexcept
{
    return std::equal(blocks_.begin(), blocks_.end(),
        other.blocks_.begin(), other.blocks_.end(),
        [](const auto& a, const auto& b)
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
    // return doc::makeOverview(blocks_);
    doc::Overview ov;
    ov.brief = getBrief(corpus);

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
            ov.returns = static_cast<
                doc::Returns const*>(it->get());
            break;
        case doc::Kind::param:
            ov.params.push_back(static_cast<
                doc::Param const*>(it->get()));
            break;
        case doc::Kind::tparam:
            ov.tparams.push_back(static_cast<
                doc::TParam const*>(it->get()));
            break;
        case doc::Kind::throws:
            ov.exceptions.push_back(static_cast<
                doc::Throws const*>(it->get()));
            break;
        default:
            if(ov.brief == it->get())
                break;
            ov.blocks.push_back(it->get());
        }
    }

    return ov;
}

std::string
Javadoc::
emplace_back(
    std::unique_ptr<doc::Block> block)
{
    MRDOCS_ASSERT(block->isBlock());

    std::string result;
    switch(block->kind)
    {
    case doc::Kind::param:
    {
        // check for duplicate parameter name
        auto t = static_cast<doc::Param const*>(block.get());
        for(auto const& q : blocks_)
        {
            if(q->kind == doc::Kind::param)
            {
                auto u = static_cast<doc::Param const*>(q.get());
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
        auto t = static_cast<doc::TParam const*>(block.get());
        for(auto const& q : blocks_)
        {
            if(q->kind == doc::Kind::tparam)
            {
                auto u = static_cast<doc::TParam const*>(q.get());
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
append(doc::List<doc::Node>&& blocks)
{
    blocks_.reserve(blocks_.size() + blocks.size());
    for(auto&& block : blocks)
        emplace_back(std::unique_ptr<doc::Block>(
            static_cast<doc::Block*>(block.release())));
    #if 0
    {
        MRDOCS_ASSERT(block->isBlock());
        blocks_.emplace_back(
            static_cast<doc::Block*>(block.release()));
    }
    #endif
}

} // mrdocs
} // clang
