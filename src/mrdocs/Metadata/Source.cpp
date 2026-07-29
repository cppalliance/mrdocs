//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Symbol/FileKind.hpp>
#include <mrdocs/Metadata/Symbol/Location.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Support/Reflection/MergeReflectedType.hpp>
#include <llvm/ADT/STLExtras.h>
#include <ranges>

namespace mrdocs {

namespace
{
template <bool Move, class SourceInfoTy>
void
mergeImpl(SourceInfo& I, SourceInfoTy&& Other)
{
    if (!I.DefLoc)
    {
        I.DefLoc = Other.DefLoc;
    }
    else if (Other.DefLoc)
    {
        if (!I.DefLoc->Documented && Other.DefLoc->Documented)
        {
            I.DefLoc = Other.DefLoc;
        }
        else
        {
            I.DefLoc = std::min(I.DefLoc, Other.DefLoc);
        }
    }
    if constexpr (Move)
    {
        std::ranges::move(Other.Loc, std::back_inserter(I.Loc));
    }
    else
    {
        std::ranges::copy(Other.Loc, std::back_inserter(I.Loc));
    }
    std::ranges::sort(I.Loc);
    auto const Last = std::ranges::unique(I.Loc).begin();
    I.Loc.erase(Last, I.Loc.end());
}
}

void
merge(SourceInfo& I, SourceInfo const& Other)
{
    mergeImpl<false>(I, Other);
}

void
merge(SourceInfo& I, SourceInfo&& Other)
{
    mergeImpl<true>(I, Other);
}

Optional<Location>
getPrimaryLocation(SourceInfo const& I, bool const preferDefinition)
{
    if (I.Loc.empty() ||
        (preferDefinition &&
        I.DefLoc))
    {
        return I.DefLoc;
    }
    auto const documentedIt = std::ranges::find_if(
            I.Loc, &Location::Documented);
    if (documentedIt != I.Loc.end())
    {
        return Optional<Location>(*documentedIt);
    }
    return Optional<Location>(I.Loc.front());
}

} // mrdocs
