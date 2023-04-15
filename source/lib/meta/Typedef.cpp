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

#include <mrdox/meta/Typedef.hpp>
#include <cassert>

namespace clang {
namespace mrdox {

void
TypedefInfo::
merge(
    TypedefInfo&& Other)
{
    assert(canMerge(Other));
    if (!IsUsing)
        IsUsing = Other.IsUsing;
    if (Underlying.Type.Name == "")
        Underlying = Other.Underlying;
    SymbolInfo::merge(std::move(Other));
}

} // mrdox
} // clang
