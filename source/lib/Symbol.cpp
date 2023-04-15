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

#include <mrdox/Symbol.hpp>
#include <llvm/ADT/STLExtras.h>
#include <cassert>

namespace clang {
namespace mrdox {

void
SymbolInfo::
merge(
    SymbolInfo&& Other)
{
    assert(canMerge(Other));
    if (!DefLoc)
        DefLoc = std::move(Other.DefLoc);
    // Unconditionally extend the list of locations, since we want all of them.
    std::move(Other.Loc.begin(), Other.Loc.end(), std::back_inserter(Loc));
    llvm::sort(Loc);
    auto Last = std::unique(Loc.begin(), Loc.end());
    Loc.erase(Last, Loc.end());
    mergeBase(std::move(Other));
}

} // mrdox
} // clang
