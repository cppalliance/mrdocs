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
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"

namespace clang {
namespace mrdox {

bool CommentInfo::operator==(const CommentInfo& Other) const {
    auto FirstCI = std::tie(Kind, Text, Name, Direction, ParamName, CloseName,
        SelfClosing, Explicit, AttrKeys, AttrValues, Args);
    auto SecondCI =
        std::tie(Other.Kind, Other.Text, Other.Name, Other.Direction,
            Other.ParamName, Other.CloseName, Other.SelfClosing,
            Other.Explicit, Other.AttrKeys, Other.AttrValues, Other.Args);

    if (FirstCI != SecondCI || Children.size() != Other.Children.size())
        return false;

    return std::equal(Children.begin(), Children.end(), Other.Children.begin(),
        llvm::deref<std::equal_to<>>{});
}

bool CommentInfo::operator<(const CommentInfo& Other) const {
    auto FirstCI = std::tie(Kind, Text, Name, Direction, ParamName, CloseName,
        SelfClosing, Explicit, AttrKeys, AttrValues, Args);
    auto SecondCI =
        std::tie(Other.Kind, Other.Text, Other.Name, Other.Direction,
            Other.ParamName, Other.CloseName, Other.SelfClosing,
            Other.Explicit, Other.AttrKeys, Other.AttrValues, Other.Args);

    if (FirstCI < SecondCI)
        return true;

    if (FirstCI == SecondCI) {
        return std::lexicographical_compare(
            Children.begin(), Children.end(), Other.Children.begin(),
            Other.Children.end(), llvm::deref<std::less<>>());
    }

    return false;
}

} // mrdox
} // clang
