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

bool
CommentInfo::
operator==(
    CommentInfo const& Other) const
{
    auto FirstCI = std::tie(
        Kind, Text, Name, Direction,
        ParamName, CloseName, SelfClosing,
        Explicit, AttrKeys, AttrValues, Args);
    auto SecondCI = std::tie(
        Other.Kind, Other.Text, Other.Name, Other.Direction,
        Other.ParamName, Other.CloseName, Other.SelfClosing,
        Other.Explicit, Other.AttrKeys, Other.AttrValues, Other.Args);
    if (FirstCI != SecondCI || Children.size() != Other.Children.size())
        return false;
    return std::equal(
        Children.begin(), Children.end(),
        Other.Children.begin(),
        llvm::deref<std::equal_to<>>{});
}

bool
CommentInfo::
operator<(
    CommentInfo const& Other) const
{
    auto FirstCI = std::tie(
        Kind, Text, Name, Direction,
        ParamName, CloseName, SelfClosing,
        Explicit, AttrKeys, AttrValues, Args);
    auto SecondCI = std::tie(
        Other.Kind, Other.Text, Other.Name, Other.Direction,
        Other.ParamName, Other.CloseName, Other.SelfClosing,
        Other.Explicit, Other.AttrKeys, Other.AttrValues, Other.Args);
    if (FirstCI < SecondCI)
        return true;
    if (FirstCI == SecondCI) {
        return std::lexicographical_compare(
            Children.begin(), Children.end(),
            Other.Children.begin(), Other.Children.end(),
            llvm::deref<std::less<>>());
    }
    return false;
}

//------------------------------------------------

Javadoc::
Javadoc(
    Paragraph brief,
    List<Block> blocks,
    List<Param> params,
    List<TParam> tparams,
    Returns returns)
    : blocks_(std::move(blocks))
    , params_(std::move(params))
    , tparams_(std::move(tparams))
    , returns_(std::move(returns))
{
    if(! brief.list.empty())
    {
        brief_ = std::make_shared<Paragraph>(std::move(brief));
        return;
    }

    // first paragraph is implicit brief
    if(! blocks_.erase_first_of_if(
        [&](Block& block)
        {
            if(block.kind != Kind::paragraph)
                return false;
            brief_ = std::make_shared<Paragraph>(std::move(
                static_cast<Paragraph&>(block)));
            return true;
        }))
    {
        brief_ = std::make_shared<Paragraph>();
    }
}

} // mrdox
} // clang
