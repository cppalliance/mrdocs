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

#include <mrdox/meta/Javadoc.hpp>
#include <mrdox/meta/Namespace.hpp>
#include <mrdox/Config.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

static_assert(std::is_copy_constructible_v<Javadoc::Node>);
static_assert(std::is_copy_constructible_v<Javadoc::Text>);
static_assert(std::is_copy_constructible_v<Javadoc::StyledText>);
static_assert(std::is_copy_constructible_v<Javadoc::Block>);
static_assert(std::is_copy_constructible_v<Javadoc::Paragraph>);
static_assert(std::is_copy_constructible_v<Javadoc::Param>);
static_assert(std::is_copy_constructible_v<Javadoc::TParam>);
static_assert(std::is_copy_constructible_v<Javadoc::Code>);

static_assert(std::is_move_constructible_v<Javadoc::Node>);
static_assert(std::is_move_constructible_v<Javadoc::Text>);
static_assert(std::is_move_constructible_v<Javadoc::StyledText>);
static_assert(std::is_move_constructible_v<Javadoc::Block>);
static_assert(std::is_move_constructible_v<Javadoc::Paragraph>);
static_assert(std::is_move_constructible_v<Javadoc::Param>);
static_assert(std::is_move_constructible_v<Javadoc::TParam>);
static_assert(std::is_move_constructible_v<Javadoc::Code>);

//------------------------------------------------

Javadoc::
Javadoc(
    List<Block> blocks,
    List<Param> params,
    List<TParam> tparams,
    Returns returns)
    : blocks_(std::move(blocks))
    , params_(std::move(params))
    , tparams_(std::move(tparams))
    , returns_(std::move(returns))
{
}

bool
Javadoc::
empty() const noexcept
{
    if( (! brief_) &&
        blocks_.empty() &&
        params_.empty() &&
        tparams_.empty() &&
        returns_.empty())
    {
        return true;
    }
    return false;
}

void
Javadoc::
merge(Javadoc& other)
{
    append(blocks_, std::move(other.blocks_));
    append(params_, std::move(other.params_));
    append(tparams_, std::move(other.tparams_));
    if( returns_.empty())
        returns_ = std::move(other.returns_);
}

void
Javadoc::
calculateBrief()
{
    Paragraph* brief = nullptr;
    for(auto& block : blocks_)
    {
        if(block.kind == Kind::brief)
        {
            brief = static_cast<Paragraph*>(&block);
            break;
        }
        if(block.kind == Kind::paragraph && ! brief)
            brief = static_cast<Paragraph*>(&block);
    }
    if(brief != nullptr)
    {
        brief_ = blocks_.extract_first_of<Paragraph>(
            [this, brief](Block& block)
            {
                return brief == &block;
            });
    }
}

} // mrdox
} // clang
