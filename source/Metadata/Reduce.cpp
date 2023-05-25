//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Reduce.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Metadata.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdox {

namespace {

struct LocationEqual
{
    bool
    operator()(
        Location const& L0,
        Location const& L1) const noexcept
    {
        return
            std::tie(L0.LineNumber, L0.Filename) ==
            std::tie(L1.LineNumber, L1.Filename);
    }
};

struct LocationLess
{
    // This operator is used to sort a vector of Locations.
    // No specific order (attributes more important than others) is required. Any
    // sort is enough, the order is only needed to call std::unique after sorting
    // the vector.
    bool operator()(
        Location const& L0,
        Location const& L1) const noexcept
    {
        return
            std::tie(L0.LineNumber, L0.Filename) <
            std::tie(L1.LineNumber, L1.Filename);
    }
};

} // (anon)

#ifndef NDEBUG
static bool canMerge(Info const& I, Info const& Other)
{
    return
        I.IT == Other.IT &&
        I.id == Other.id;
}

static bool canMerge(Reference const& I, Reference const& Other)
{
    return
        I.RefType == Other.RefType &&
        I.id == Other.id;
}
#endif

static void merge(Javadoc& I, Javadoc&& other)
{
    // FIXME: this doesn't merge parameter information;
    // parameters with the same name but different direction
    // or descriptions end up being duplicated
    if(other != I)
    {
        // Unconditionally extend the blocks
        // since each decl may have a comment.
        I.append(I.getBlocks(), std::move(other.getBlocks()));
    }
}

void mergeInfo(Info& I, Info&& Other)
{
    Assert(canMerge(I, Other));
    if (I.id == SymbolID::zero)
        I.id = Other.id;
    if (I.Name == "")
        I.Name = Other.Name;
    if (I.Namespace.empty())
        I.Namespace = std::move(Other.Namespace);

    // append javadocs
    if(! I.javadoc)
        I.javadoc = std::move(Other.javadoc);
    else if(Other.javadoc)
        merge(*I.javadoc, std::move(*Other.javadoc));
}

static void mergeSymbolInfo(SymbolInfo& I, SymbolInfo&& Other)
{
    Assert(canMerge(I, Other));
    if (!I.DefLoc)
        I.DefLoc = std::move(Other.DefLoc);
    // Unconditionally extend the list of locations, since we want all of them.
    std::move(Other.Loc.begin(), Other.Loc.end(), std::back_inserter(I.Loc));
    // VFALCO This has the fortuituous effect of also canonicalizing
    llvm::sort(I.Loc, LocationLess{});
    auto Last = std::unique(I.Loc.begin(), I.Loc.end(), LocationEqual{});
    I.Loc.erase(Last, I.Loc.end());
    mergeInfo(I, std::move(Other));
}

void merge(NamespaceInfo& I, NamespaceInfo&& Other)
{
    Assert(canMerge(I, Other));
    reduceChildren(I.Children.Namespaces, std::move(Other.Children.Namespaces));
    reduceChildren(I.Children.Records, std::move(Other.Children.Records));
    reduceChildren(I.Children.Functions, std::move(Other.Children.Functions));
    reduceChildren(I.Children.Typedefs, std::move(Other.Children.Typedefs));
    reduceChildren(I.Children.Enums, std::move(Other.Children.Enums));
    reduceChildren(I.Children.Vars, std::move(Other.Children.Vars));
    mergeInfo(I, std::move(Other));
}

static
void
reduceRefsWithAccess(
    std::vector<MemberRef>& list,
    std::vector<MemberRef>&& otherList)
{
    for(auto const& ref : otherList)
    {
        auto it = llvm::find_if(
            list,
            [ref](MemberRef const& other) noexcept
            {
                return other.id == ref.id;
            });
        if(it != list.end())
        {
            Assert(it->access == ref.access);
            continue;
        }
        list.push_back(ref);
    }
}

void merge(RecordInfo& I, RecordInfo&& Other)
{
    Assert(canMerge(I, Other));
    if (!I.TagType)
        I.TagType = Other.TagType;
    I.IsTypeDef = I.IsTypeDef || Other.IsTypeDef;
    I.specs.raw.value |= Other.specs.raw.value;
    if (I.Members.empty())
        I.Members = std::move(Other.Members);
    if (I.Bases.empty())
        I.Bases = std::move(Other.Bases);
    if( I.Bases.empty())
        I.Bases = std::move(Other.Bases);
    // Reduce children if necessary.
    reduceRefsWithAccess(I.Children_.Records, std::move(Other.Children_.Records));
    reduceRefsWithAccess(I.Children_.Functions, std::move(Other.Children_.Functions));
    reduceRefsWithAccess(I.Children_.Enums, std::move(Other.Children_.Enums));
    reduceRefsWithAccess(I.Children_.Types, std::move(Other.Children_.Types));
    reduceRefsWithAccess(I.Children_.Vars, std::move(Other.Children_.Vars));
    mergeSymbolInfo(I, std::move(Other));
    if (!I.Template)
        I.Template = Other.Template;
    // This has the side-effect of canonicalizing
    if(! Other.Friends.empty())
    {
        llvm::append_range(I.Friends, std::move(Other.Friends));
        llvm::sort(I.Friends,
            [](SymbolID const& id0, SymbolID const& id1)
            {
                return id0 < id1;
            });
        auto last = std::unique(I.Friends.begin(), I.Friends.end());
        I.Friends.erase(last, I.Friends.end());
    }
}

void merge(FunctionInfo& I, FunctionInfo&& Other)
{
    Assert(canMerge(I, Other));
    if (I.ReturnType.Type.id == SymbolID::zero && I.ReturnType.Type.Name == "")
        I.ReturnType = std::move(Other.ReturnType);
    if (I.Params.empty())
        I.Params = std::move(Other.Params);
    mergeSymbolInfo(I, std::move(Other));
    if (!I.Template)
        I.Template = Other.Template;
    I.specs0.raw.value |= Other.specs0.raw.value;
    I.specs1.raw.value |= Other.specs1.raw.value;
}

void merge(TypedefInfo& I, TypedefInfo&& Other)
{
    Assert(canMerge(I, Other));
    if (!I.IsUsing)
        I.IsUsing = Other.IsUsing;
    if (I.Underlying.Type.Name == "")
        I.Underlying = Other.Underlying;
    mergeSymbolInfo(I, std::move(Other));
}

void merge(EnumInfo& I, EnumInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(! I.Scoped)
        I.Scoped = Other.Scoped;
    if (I.Members.empty())
        I.Members = std::move(Other.Members);
    mergeSymbolInfo(I, std::move(Other));
}

void merge(VarInfo& I, VarInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(I.Type.id == SymbolID::zero && I.Type.Name.empty())
        I.Type = std::move(Other.Type);
    mergeSymbolInfo(I, std::move(Other));
    I.specs.raw.value |= Other.specs.raw.value;
}

void merge(Reference& I, Reference&& Other)
{
    Assert(canMerge(I, Other));
    if (I.Name.empty())
        I.Name = Other.Name;
}

} // mrdox
} // clang
