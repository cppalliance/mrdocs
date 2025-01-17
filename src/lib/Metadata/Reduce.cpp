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
            std::tie(L0.LineNumber, L0.FullPath) ==
            std::tie(L1.LineNumber, L1.FullPath);
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
            std::tie(L0.LineNumber, L0.FullPath) <
            std::tie(L1.LineNumber, L1.FullPath);
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

void
mergeInfo(Info& I, Info&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    MRDOCS_ASSERT(I.id);
    if (I.Name == "")
    {
        I.Name = Other.Name;
    }
    if (I.Parent)
    {
        I.Parent = std::move(Other.Parent);
    }
    if (I.Access == AccessKind::None)
    {
        I.Access = Other.Access;
    }
    I.Extraction = leastSpecific(I.Extraction, Other.Extraction);

    // Append javadocs
    if (!I.javadoc)
    {
        I.javadoc = std::move(Other.javadoc);
    }
    else if (Other.javadoc)
    {
        merge(*I.javadoc, std::move(*Other.javadoc));
    }
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
    mergeScopeInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    reduceSymbolIDs(I.UsingDirectives,
        std::move(Other.UsingDirectives));

    I.IsInline |= Other.IsInline;
    I.IsAnonymous |= Other.IsAnonymous;
}

void merge(RecordInfo& I, RecordInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(Other.KeyKind != RecordKeyKind::Struct &&
        I.KeyKind != Other.KeyKind)
        I.KeyKind = Other.KeyKind;
    I.IsTypeDef |= Other.IsTypeDef;
    I.IsFinal |= Other.IsFinal;
    I.IsFinalDestructor |= Other.IsFinalDestructor;

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
    if(I.Noexcept.Implicit)
        I.Noexcept = std::move(Other.Noexcept);
    if(I.Explicit.Implicit)
        I.Explicit = std::move(Other.Explicit);
    mergeExprInfo(I.Requires, std::move(Other.Requires));

    I.IsVariadic |= Other.IsVariadic;
    I.IsVirtual |= Other.IsVirtual;
    I.IsVirtualAsWritten |= Other.IsVirtualAsWritten;
    I.IsPure |= Other.IsPure;
    I.IsDefaulted |= Other.IsDefaulted;
    I.IsExplicitlyDefaulted |= Other.IsExplicitlyDefaulted;
    I.IsDeleted |= Other.IsDeleted;
    I.IsDeletedAsWritten |= Other.IsDeletedAsWritten;
    I.IsNoReturn |= Other.IsNoReturn;
    I.HasOverrideAttr |= Other.HasOverrideAttr;
    I.HasTrailingReturn |= Other.HasTrailingReturn;
    I.IsConst |= Other.IsConst;
    I.IsVolatile |= Other.IsVolatile;
    I.IsFinal |= Other.IsFinal;
    I.IsNodiscard |= Other.IsNodiscard;
    I.IsExplicitObjectMemberFunction |= Other.IsExplicitObjectMemberFunction;

    if(I.Constexpr == ConstexprKind::None)
        I.Constexpr = Other.Constexpr;
    if(I.StorageClass == StorageClassKind::None)
        I.StorageClass = Other.StorageClass;
    if(I.RefQualifier == ReferenceKind::None)
        I.RefQualifier = Other.RefQualifier;
    if(I.OverloadedOperator == OperatorKind::None)
        I.OverloadedOperator = Other.OverloadedOperator;
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
    if(I.Explicit.Implicit)
        I.Explicit = std::move(Other.Explicit);
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
    if(I.Default.Written.empty())
        I.Default = std::move(Other.Default);

    I.IsBitfield |= Other.IsBitfield;
    mergeExprInfo(I.BitfieldWidth,
        std::move(Other.BitfieldWidth));

    I.IsVariant |= Other.IsVariant;
    I.IsMutable |= Other.IsMutable;
    I.IsMaybeUnused |= Other.IsMaybeUnused;
    I.IsDeprecated |= Other.IsDeprecated;
    I.HasNoUniqueAddress |= Other.HasNoUniqueAddress;
}

void merge(VariableInfo& I, VariableInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(! I.Type)
        I.Type = std::move(Other.Type);
    if(! I.Template)
        I.Template = std::move(Other.Template);
    if(I.Initializer.Written.empty())
        I.Initializer = std::move(Other.Initializer);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));

    I.IsConstinit |= Other.IsConstinit;
    I.IsThreadLocal |= Other.IsThreadLocal;
    I.IsConstexpr |= Other.IsConstexpr;
    I.IsInline |= Other.IsInline;
    if (I.StorageClass == StorageClassKind::None)
    {
        I.StorageClass = Other.StorageClass;
    }
    for (auto& otherAttr: Other.Attributes)
    {
        if (std::ranges::find(I.Attributes, otherAttr) == I.Attributes.end())
        {
            I.Attributes.push_back(otherAttr);
        }
    }
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

void merge(NamespaceAliasInfo& I, NamespaceAliasInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if (! I.AliasedSymbol)
        I.AliasedSymbol = std::move(Other.AliasedSymbol);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(UsingInfo& I, UsingInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));

    reduceSymbolIDs(I.UsingSymbols, std::move(Other.UsingSymbols));
    if (I.Class == UsingClass::Normal)
        I.Class = Other.Class;
    if (! I.Qualifier)
        I.Qualifier = std::move(Other.Qualifier);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(EnumConstantInfo& I, EnumConstantInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(I.Initializer.Written.empty())
        I.Initializer = std::move(Other.Initializer);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
}

void merge(ConceptInfo& I, ConceptInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    if(I.Constraint.Written.empty())
        I.Constraint = std::move(Other.Constraint);
    mergeSourceInfo(I, std::move(Other));
    mergeInfo(I, std::move(Other));
    if (! I.Template)
        I.Template = std::move(Other.Template);
}

} // mrdocs
} // clang
