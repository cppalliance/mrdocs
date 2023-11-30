//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Reduce.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Platform.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdocs {

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

void
reduceSymbolIDs(
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

void
reduceLookups(
    std::unordered_map<std::string, std::vector<SymbolID>>& I,
    std::unordered_map<std::string, std::vector<SymbolID>>&& Other)
{
    I.merge(std::move(Other));
    for(auto& [name, ids] : Other)
    {
        auto [it, created] = I.try_emplace(name);
        reduceSymbolIDs(it->second, std::move(ids));
    }
}

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
        I.append(std::move(other));
    }
}

void mergeInfo(Info& I, Info&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    MRDOCS_ASSERT(I.id);
    if(I.Name == "")
        I.Name = Other.Name;
    if(I.Namespace.empty())
        I.Namespace = std::move(Other.Namespace);
    if(I.Access == AccessKind::None)
        I.Access = Other.Access;
    I.Implicit &= Other.Implicit;
    // append javadocs
    if(! I.javadoc)
        I.javadoc = std::move(Other.javadoc);
    else if(Other.javadoc)
        merge(*I.javadoc, std::move(*Other.javadoc));
}

void mergeScopeInfo(ScopeInfo& I, ScopeInfo&& Other)
{
    reduceSymbolIDs(I.Members, std::move(Other.Members));
    reduceLookups(I.Lookups, std::move(Other.Lookups));
}

static void mergeSourceInfo(
    SourceInfo& I,
    SourceInfo&& Other)
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

static void mergeExprInfo(
    ExprInfo& I,
    ExprInfo&& Other)
{
    if(I.Written.empty())
        I.Written = std::move(Other.Written);
}

template<typename T>
static void mergeExprInfo(
    ConstantExprInfo<T>& I,
    ConstantExprInfo<T>&& Other)
{
    mergeExprInfo(
        static_cast<ExprInfo&>(I),
        static_cast<ExprInfo&&>(Other));
    if(! I.Value)
        I.Value = std::move(Other.Value);
}

void merge(NamespaceInfo& I, NamespaceInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    I.specs.raw.value |= Other.specs.raw.value;
    mergeScopeInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(RecordInfo& I, RecordInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(Other.KeyKind != RecordKeyKind::Struct &&
        I.KeyKind != Other.KeyKind)
        I.KeyKind = Other.KeyKind;
    I.IsTypeDef = I.IsTypeDef || Other.IsTypeDef;
    I.specs.raw.value |= Other.specs.raw.value;
    if (I.Bases.empty())
        I.Bases = std::move(Other.Bases);
    // KRYSTIAN FIXME: really should use explicit cases here.
    // at a glance, it is not obvious that we are binding to
    // the SymboInfo base class subobject
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    mergeScopeInfo(I, std::move(Other));
    if (! I.Template)
        I.Template = std::move(Other.Template);
}

void merge(FunctionInfo& I, FunctionInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(I.Class == FunctionClass::Normal)
        I.Class = Other.Class;
    if (! I.ReturnType)
        I.ReturnType = std::move(Other.ReturnType);
    if (I.Params.empty())
        I.Params = std::move(Other.Params);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    if (!I.Template)
        I.Template = std::move(Other.Template);
    I.specs0.raw.value |= Other.specs0.raw.value;
    I.specs1.raw.value |= Other.specs1.raw.value;
}

void merge(GuideInfo& I, GuideInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));

    if (! I.Deduced)
        I.Deduced = std::move(Other.Deduced);
    if (I.Params.empty())
        I.Params = std::move(Other.Params);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    if (!I.Template)
        I.Template = std::move(Other.Template);
    if(I.Explicit == ExplicitKind::None)
        I.Explicit = Other.Explicit;
}

void merge(TypedefInfo& I, TypedefInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if (!I.IsUsing)
        I.IsUsing = Other.IsUsing;
    if (! I.Type)
        I.Type = std::move(Other.Type);
    if(! I.Template)
        I.Template = std::move(Other.Template);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(EnumInfo& I, EnumInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(! I.Scoped)
        I.Scoped = Other.Scoped;
    if (! I.UnderlyingType)
        I.UnderlyingType = std::move(Other.UnderlyingType);
    mergeScopeInfo(I, std::move(Other));
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(FieldInfo& I, FieldInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(! I.Type)
        I.Type = std::move(Other.Type);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    I.specs.raw.value |= Other.specs.raw.value;
    I.IsMutable |= Other.IsMutable;
    if(I.Default.empty())
        I.Default = std::move(Other.Default);

    I.IsBitfield |= Other.IsBitfield;
    mergeExprInfo(I.BitfieldWidth,
        std::move(Other.BitfieldWidth));
}

void merge(VariableInfo& I, VariableInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(! I.Type)
        I.Type = std::move(Other.Type);
    if(! I.Template)
        I.Template = std::move(Other.Template);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    I.specs.raw.value |= Other.specs.raw.value;
}

void merge(SpecializationInfo& I, SpecializationInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(! I.Primary)
        I.Primary = Other.Primary;
    if(I.Args.empty())
        I.Args = std::move(Other.Args);
    mergeInfo(I, std::move(Other));
    mergeScopeInfo(I, std::move(Other));
}

void merge(FriendInfo& I, FriendInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    if(! I.FriendSymbol)
        I.FriendSymbol = Other.FriendSymbol;
    if(! I.FriendType)
        I.FriendType = std::move(Other.FriendType);
}

void merge(EnumeratorInfo& I, EnumeratorInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(I.Initializer.Written.empty())
        I.Initializer = std::move(Other.Initializer);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

} // mrdocs
} // clang
