//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Debug.hpp"
#include <mrdox/Metadata/Javadoc.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

template<class T>
concept a_Node =
    std::is_copy_constructible_v<T> &&
    std::three_way_comparable<T>;

static_assert(a_Node<doc::Node>);
static_assert(a_Node<doc::Text>);
static_assert(a_Node<doc::StyledText>);
static_assert(a_Node<doc::Block>);
static_assert(a_Node<doc::Paragraph>);
static_assert(a_Node<doc::Param>);
static_assert(a_Node<doc::TParam>);
static_assert(a_Node<doc::Code>);

//------------------------------------------------

Javadoc::
Javadoc() noexcept = default;

Javadoc::
Javadoc(
    AnyList<doc::Block> blocks)
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
    return
        std::tie(brief_, blocks_) ==
        std::tie(other.brief_, other.blocks_);
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
        if(it->kind == doc::Kind::brief)
        {
            brief = static_cast<doc::Paragraph*>(&*it);
            goto done;
        }
        else if(it->kind == doc::Kind::returns)
        {
            if(! returns_)
                returns_ = std::make_shared<doc::Returns>(
                    std::move(static_cast<doc::Returns &>(*it)));
            // unconditionally consume the Returns element
            it = blocks_.erase(it);
            // KRYSTIAN TODO: emit a warning for duplicate @returns
            continue;
        }
        else if(it->kind == doc::Kind::param)
        {
            it = blocks_.move_to(it, params_);
            continue;
        }
        else if(it->kind == doc::Kind::tparam)
        {
            it = blocks_.move_to(it, tparams_);
            continue;
        }
        if(it->kind == doc::Kind::paragraph && ! brief)
        {
            brief = static_cast<doc::Paragraph*>(&*it);
            ++it;
            goto find_brief;
        }
        ++it;
    }
    goto done;
find_brief:
    while(it != blocks_.end())
    {
        if(it->kind == doc::Kind::brief)
        {
            brief = static_cast<doc::Paragraph*>(&*it);
            break;
        }
        else if(it->kind == doc::Kind::param)
        {
            it = blocks_.move_to(it, params_);
            continue;
        }
        else if(it->kind == doc::Kind::tparam)
        {
            it = blocks_.move_to(it, tparams_);
            continue;
        }
        ++it;
    }
done:
    if(brief != nullptr)
    {
        brief_ = blocks_.extract_first_of<doc::Paragraph>(
            [brief](doc::Block& block)
            {
                return brief == &block;
            });
    }
    else
    {
        static std::shared_ptr<doc::Paragraph const> empty_para =
            std::make_shared<doc::Paragraph>();
        brief_ = empty_para;
    }
}

} // mrdox
} // clang
