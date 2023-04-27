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

#include <mrdox/Debug.hpp>
#include <mrdox/Metadata//Function.hpp>
#include <utility>

namespace clang {
namespace mrdox {

void
FunctionInfo::
merge(
    FunctionInfo&& Other)
{
    Assert(canMerge(Other));
    if (!IsMethod)
        IsMethod = Other.IsMethod;
    if (!Access)
        Access = Other.Access;
    if (ReturnType.Type.id == EmptySID && ReturnType.Type.Name == "")
        ReturnType = std::move(Other.ReturnType);
    if (Parent.id == EmptySID && Parent.Name == "")
        Parent = std::move(Other.Parent);
    if (Params.empty())
        Params = std::move(Other.Params);
    SymbolInfo::merge(std::move(Other));
    if (!Template)
        Template = Other.Template;
    specs0.merge(std::move(Other).specs0);
    specs1.merge(std::move(Other).specs1);
}

} // mrdox
} // clang

