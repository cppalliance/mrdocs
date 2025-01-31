//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <ranges>
#include <llvm/ADT/STLExtras.h>
#include <mrdocs/Metadata/Source.hpp>

namespace clang::mrdocs {

std::string_view
toString(FileKind kind)
{
    switch(kind)
    {
    case FileKind::Source:
        return "source";
    case FileKind::System:
        return "system";
    case FileKind::Other:
        return "other";
    default:
        MRDOCS_UNREACHABLE();
    };
}

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

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Location const& loc)
{
    io.map("fullPath", loc.FullPath);
    io.map("shortPath", loc.ShortPath);
    io.map("sourcePath", loc.SourcePath);
    io.map("line", loc.LineNumber);
    io.map("documented", loc.Documented);
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Location const& loc)
{
    v = dom::LazyObject(loc);
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    SourceInfo const& I)
{
    if (I.DefLoc)
    {
        io.map("def", *I.DefLoc);
    }
    if (!I.Loc.empty())
    {
        io.map("decl", dom::LazyArray(I.Loc));
    }
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SourceInfo const& I)
{
    v = dom::LazyObject(I);
}

} // clang::mrdocs
