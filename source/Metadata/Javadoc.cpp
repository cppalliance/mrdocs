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

namespace clang {
namespace mrdox {

namespace doc {

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
            ov.brief = static_cast<
                Paragraph const*>(it->get());
            break;
        default:
            ov.blocks.push_back(it->get());
            break;
        }
    }

    return ov;
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

bool
Javadoc::
empty() const noexcept
{
    if( ! brief_ &&
        blocks_.empty())
    {
        return true;
    }
    return false;
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

void
Javadoc::
postProcess()
{
    doc::Paragraph* brief = nullptr;
    auto it = blocks_.begin();
    while(it != blocks_.end())
    {
        auto& block_ptr = *it;
        if(block_ptr->kind == doc::Kind::brief)
        {
            brief = static_cast<doc::Paragraph*>(block_ptr.get());
            goto done;
        }
        else if(block_ptr->kind == doc::Kind::returns)
        {
            if(! returns_)
                returns_.reset(static_cast<
                    doc::Returns*>(block_ptr.release()));
            // unconditionally consume the Returns element
            it = blocks_.erase(it);
            // KRYSTIAN TODO: emit a warning for duplicate @returns
            continue;
        }
        else if(block_ptr->kind == doc::Kind::param)
        {
            it = move_to(params_, blocks_, it);
            continue;
        }
        else if(block_ptr->kind == doc::Kind::tparam)
        {
            it = move_to(tparams_, blocks_, it);
            continue;
        }
        if(block_ptr->kind == doc::Kind::paragraph && ! brief)
        {
            brief = static_cast<doc::Paragraph*>(block_ptr.get());
            ++it;
            goto find_brief;
        }
        ++it;
    }
    goto done;
find_brief:
    while(it != blocks_.end())
    {
        auto& block_ptr = *it;
        if(block_ptr->kind == doc::Kind::brief)
        {
            brief = static_cast<doc::Paragraph*>(block_ptr.get());
            break;
        }
        else if(block_ptr->kind == doc::Kind::param)
        {
            it = move_to(params_, blocks_, it);
            continue;
        }
        else if(block_ptr->kind == doc::Kind::tparam)
        {
            it = move_to(tparams_, blocks_, it);
            continue;
        }
        ++it;
    }
done:
    if(brief != nullptr)
    {
        auto match = std::find_if(blocks_.begin(), blocks_.end(),
            [brief](std::unique_ptr<doc::Block>& block)
            {
                return brief == block.get();
            });
        MRDOX_ASSERT(match != blocks_.end());
        brief_.reset(static_cast<doc::Paragraph*>(
            match->release()));
        blocks_.erase(match);
    }
    // KRYSTIAN FIXME: should an empty paragraph be used
    // as the brief when no written brief exists?
}

//------------------------------------------------

doc::Overview
Javadoc::
makeOverview() const
{
    return doc::makeOverview(blocks_);
}

} // mrdox
} // clang
