//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info/Using.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang::mrdocs {

namespace {

void
reduceSymbolIDs(
    std::vector<SymbolID>& list,
    std::vector<SymbolID>&& otherList)
{
    for(auto const& id : otherList)
    {
        if (auto it = llvm::find(list, id); it != list.end())
        {
            continue;
        }
        list.push_back(id);
    }
}

} // (anon)

void
merge(UsingInfo& I, UsingInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(I.asInfo(), std::move(Other.asInfo()));
    reduceSymbolIDs(I.ShadowDeclarations, std::move(Other.ShadowDeclarations));
    if (I.Class == UsingClass::Normal)
    {
        I.Class = Other.Class;
    }
    if (I.IntroducedName.valueless_after_move() ||
        I.IntroducedName->Name.empty())
    {
        I.IntroducedName = std::move(Other.IntroducedName);
    }
}

} // clang::mrdocs

