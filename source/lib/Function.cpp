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

#include <mrdox/meta/Function.hpp>
#include <utility>

namespace clang {
namespace mrdox {

void
FunctionInfo::
merge(
    FunctionInfo&& Other)
{
    assert(canMerge(Other));
    if (!IsMethod)
        IsMethod = Other.IsMethod;
    if (!Access)
        Access = Other.Access;
    if (ReturnType.Type.USR == EmptySID && ReturnType.Type.Name == "")
        ReturnType = std::move(Other.ReturnType);
    if (Parent.USR == EmptySID && Parent.Name == "")
        Parent = std::move(Other.Parent);
    if (Params.empty())
        Params = std::move(Other.Params);
    SymbolInfo::merge(std::move(Other));
    if (!Template)
        Template = Other.Template;
}

} // mrdox
} // clang

