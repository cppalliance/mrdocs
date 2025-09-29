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
#include <mrdocs/Metadata/Symbol/Enum.hpp>
#include <llvm/ADT/STLExtras.h>

namespace mrdocs {

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
merge(EnumSymbol& I, EnumSymbol&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(I.asInfo(), std::move(Other.asInfo()));
    if (!I.Scoped)
    {
        I.Scoped = Other.Scoped;
    }
    if (!I.UnderlyingType)
    {
        I.UnderlyingType = std::move(Other.UnderlyingType);
    }
    reduceSymbolIDs(I.Constants, std::move(Other.Constants));
}

} // mrdocs

