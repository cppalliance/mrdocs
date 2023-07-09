//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Debug.hpp"
#include <mrdox/Metadata/Javadoc.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdox {

namespace doc {

bool
Node::
isBlock() const noexcept
{
    switch(kind)
    {
    case Kind::text:
    case Kind::link:
    case Kind::styled:
        return false;

    case Kind::admonition:
    case Kind::brief:
    case Kind::code:
    case Kind::heading:
    case Kind::list_item:
    case Kind::paragraph:
    case Kind::param:
    case Kind::returns:
    case Kind::tparam:
        return true;

    default:
        MRDOX_UNREACHABLE();
    }
}

bool
Node::
isText() const noexcept
{
    switch(kind)
    {
    case Kind::text:
    case Kind::link:
    case Kind::styled:
        return true;

    case Kind::admonition:
    case Kind::brief:
    case Kind::code:
    case Kind::heading:
    case Kind::list_item:
    case Kind::paragraph:
    case Kind::param:
    case Kind::returns:
    case Kind::tparam:
        return false;

    default:
        MRDOX_UNREACHABLE();
    }
}

void
Block::
emplace_back(
    std::unique_ptr<Text> text)
{
    MRDOX_ASSERT(text->isText());
    children.emplace_back(std::move(text));
}

void
Block::
append(List<Node>&& blocks)
{
    children.reserve(children.size() + blocks.size());
    for(auto&& block : blocks)
    {
        MRDOX_ASSERT(block->isText());
        emplace_back(std::unique_ptr<Text>(
            static_cast<Text*>(block.release())));
    }
}

static
Overview
makeOverview(
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
            ov.brief = static_cast<
                Paragraph const*>(it->get());
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
        case Kind::paragraph:
            if(! ov.brief)
                ov.brief = static_cast<
                    Paragraph const*>(it->get());
            else
                ov.blocks.push_back(it->get());
            break;
        default:
            ov.blocks.push_back(it->get());
            break;
        }
    }

    return ov;
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
        MRDOX_UNREACHABLE();
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
brief() const noexcept
{
    if(brief_)
        return brief_;
    for(auto const& block : blocks_)
        if(block->kind == doc::Kind::paragraph)
            return static_cast<
                doc::Paragraph const*>(block.get());
    return nullptr;
}

bool
Javadoc::
operator==(
    Javadoc const& other) const noexcept
{
    if(! brief_ || ! other.brief_)
    {
        if(brief_ != other.brief_)
            return false;
    }
    else if(! brief_->equals(*other.brief_))
    {
        return false;
    }
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
makeOverview() const
{
    return doc::makeOverview(blocks_);
}

std::string
Javadoc::
emplace_back(
    std::unique_ptr<doc::Block> block)
{
    MRDOX_ASSERT(block->isBlock());

    std::string result;
    switch(block->kind)
    {
    case doc::Kind::brief:
    {
        // check for multiple briefs
        if(brief_ != nullptr)
            result = "multiple briefs";
        else
            brief_ = static_cast<
                doc::Paragraph const*>(block.get());
        break;
    }
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
    {
        MRDOX_ASSERT(block->isBlock());
        blocks_.emplace_back(
            static_cast<doc::Block*>(block.release()));
    }
}

} // mrdox
} // clang
