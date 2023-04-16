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

Javadoc::Paragraph const Javadoc::s_empty_;

Javadoc::
Javadoc() noexcept
    : brief_(&s_empty_)
{
}

Javadoc::
Javadoc(
    List<Block> blocks,
    List<Param> params,
    List<TParam> tparams,
    Returns returns)
    : brief_(&s_empty_)
    , blocks_(std::move(blocks))
    , params_(std::move(params))
    , tparams_(std::move(tparams))
    , returns_(std::move(returns))
{
}

auto
Javadoc::
getBrief() const noexcept ->
    Paragraph const*
{
    Paragraph const* first = nullptr;
    for(auto const& block : blocks_)
    {
        if(block.kind == Kind::brief)
            return static_cast<Paragraph const*>(&block);
        if( block.kind == Kind::paragraph && ! first)
            first = static_cast<Paragraph const*>(&block);
    }
    if(first == nullptr)
        first = &s_empty_;
    return first;
}

void
Javadoc::
merge(Javadoc& other)
{
    blocks_.splice_back(other.blocks_);
    params_.splice_back(other.params_);
    tparams_.splice_back(other.tparams_);
    if( returns_.empty())
        returns_ = std::move(other.returns_);
}


} // mrdox
} // clang
