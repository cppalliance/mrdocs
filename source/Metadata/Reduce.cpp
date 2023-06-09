//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
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
        I.Kind == Other.Kind &&
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

static void mergeSymbolInfo(
    SymbolInfo& I,
    SymbolInfo&& Other)
{
    if (! I.DefLoc)
        I.DefLoc = std::move(Other.DefLoc);
    // Unconditionally extend the list of locations, since we want all of them.
    std::move(Other.Loc.begin(), Other.Loc.end(), std::back_inserter(I.Loc));
    // VFALCO This has the fortuituous effect of also canonicalizing
    llvm::sort(I.Loc, LocationLess{});
    auto Last = std::unique(I.Loc.begin(), I.Loc.end(), LocationEqual{});
    I.Loc.erase(Last, I.Loc.end());
}


static
void
reduceRefs(
    std::vector<SymbolID>& list,
    std::vector<SymbolID>&& otherList)
{
    for(auto const& id : otherList)
    {
        auto it = llvm::find(list, id);
        if(it != list.end())
            continue;
        list.push_back(id);
    }
}

static
void
reduceMemberRefs(
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

static
void
reduceSpecializedMembers(
    std::vector<SpecializedMember>& list,
    std::vector<SpecializedMember>&& otherList)
{
    for(auto const& ref : otherList)
    {
        auto it = llvm::find_if(
            list,
            [ref](SpecializedMember const& other) noexcept
            {
                return other.Specialized == ref.Specialized;
            });
        if(it != list.end())
            continue;
        list.push_back(ref);
    }
}

void merge(NamespaceInfo& I, NamespaceInfo&& Other)
{
    Assert(canMerge(I, Other));
    reduceRefs(I.Members, std::move(Other.Members));
    reduceRefs(I.Specializations, std::move(Other.Specializations));
    mergeInfo(I, std::move(Other));
}

void merge(RecordInfo& I, RecordInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(Other.KeyKind != RecordKeyKind::Struct &&
        I.KeyKind != Other.KeyKind)
        I.KeyKind = Other.KeyKind;
    I.IsTypeDef = I.IsTypeDef || Other.IsTypeDef;
    I.specs.raw.value |= Other.specs.raw.value;
    if (I.Bases.empty())
        I.Bases = std::move(Other.Bases);
    // Reduce children if necessary.
    reduceMemberRefs(I.Members.Records, std::move(Other.Members.Records));
    reduceMemberRefs(I.Members.Functions, std::move(Other.Members.Functions));
    reduceMemberRefs(I.Members.Enums, std::move(Other.Members.Enums));
    reduceMemberRefs(I.Members.Types, std::move(Other.Members.Types));
    reduceMemberRefs(I.Members.Fields, std::move(Other.Members.Fields));
    reduceMemberRefs(I.Members.Vars, std::move(Other.Members.Vars));

    reduceRefs(I.Members.Specializations, std::move(Other.Members.Specializations));
    // KRYSTIAN FIXME: really should use explicit cases here.
    // at a glance, it is not obvious that we are binding to
    // the SymboInfo base class subobject
    mergeSymbolInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    if (! I.Template)
        I.Template = std::move(Other.Template);
#if 0
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
#else
    reduceRefs(I.Friends, std::move(Other.Friends));
#endif
}

void merge(FunctionInfo& I, FunctionInfo&& Other)
{
    Assert(canMerge(I, Other));
    if (I.ReturnType.id == SymbolID::zero && I.ReturnType.Name == "")
        I.ReturnType = std::move(Other.ReturnType);
    if (I.Params.empty())
        I.Params = std::move(Other.Params);
    mergeSymbolInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    if (!I.Template)
        I.Template = std::move(Other.Template);
    I.specs0.raw.value |= Other.specs0.raw.value;
    I.specs1.raw.value |= Other.specs1.raw.value;
}

void merge(TypedefInfo& I, TypedefInfo&& Other)
{
    Assert(canMerge(I, Other));
    if (!I.IsUsing)
        I.IsUsing = Other.IsUsing;
    if (I.Underlying.Name == "")
        I.Underlying = Other.Underlying;
    if(! I.Template)
        I.Template = std::move(Other.Template);
    mergeSymbolInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(EnumInfo& I, EnumInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(! I.Scoped)
        I.Scoped = Other.Scoped;
    if (I.Members.empty())
        I.Members = std::move(Other.Members);
    mergeSymbolInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(FieldInfo& I, FieldInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(I.Type.id == SymbolID::zero && I.Type.Name.empty())
        I.Type = std::move(Other.Type);
    mergeSymbolInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    I.specs.raw.value |= Other.specs.raw.value;
    if(I.Default.empty())
        I.Default = std::move(Other.Default);
}

void merge(VarInfo& I, VarInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(I.Type.id == SymbolID::zero && I.Type.Name.empty())
        I.Type = std::move(Other.Type);
    if(! I.Template)
        I.Template = std::move(Other.Template);
    mergeSymbolInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    I.specs.raw.value |= Other.specs.raw.value;
}

void merge(SpecializationInfo& I, SpecializationInfo&& Other)
{
    Assert(canMerge(I, Other));
    if(I.Primary == SymbolID::zero)
        I.Primary = Other.Primary;
    if(I.Args.empty())
        I.Args = std::move(Other.Args);
    reduceSpecializedMembers(I.Members,
        std::move(Other.Members));

    mergeInfo(I, std::move(Other));

}


} // mrdox
} // clang
