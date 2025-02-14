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

#include <mrdocs/Metadata/Info/Namespace.hpp>
#include <mrdocs/Platform.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang::mrdocs {

std::strong_ordering
NamespaceInfo::
operator<=>(NamespaceInfo const& other) const
{
    if (auto const res = dynamic_cast<Info const&>(*this) <=> dynamic_cast<Info const&>(other);
        std::is_neq(res))
    {
        return res;
    }
    if (auto const res = IsInline <=> other.IsInline;
        std::is_neq(res))
    {
        return res;
    }
    if (auto const res = IsAnonymous <=> other.IsAnonymous;
        std::is_neq(res))
    {
        return res;
    }
    if (auto const res = UsingDirectives.size() <=> other.UsingDirectives.size();
        std::is_neq(res))
    {
        return res;
    }
    for (auto const& [lhs, rhs] : llvm::zip(UsingDirectives, other.UsingDirectives))
    {
        if (auto const res = lhs <=> rhs;
            std::is_neq(res))
        {
            return res;
        }
    }
    if (auto const res = Members <=> other.Members;
        std::is_neq(res))
    {
        return res;
    }
    return std::strong_ordering::equal;
}

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

void
reduceNames(
    std::vector<NameInfo>& list,
    std::vector<NameInfo>&& otherList)
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
merge(NamespaceTranche& I, NamespaceTranche&& Other)
{
    reduceSymbolIDs(I.Namespaces, std::move(Other.Namespaces));
    reduceSymbolIDs(I.NamespaceAliases, std::move(Other.NamespaceAliases));
    reduceSymbolIDs(I.Typedefs, std::move(Other.Typedefs));
    reduceSymbolIDs(I.Records, std::move(Other.Records));
    reduceSymbolIDs(I.Enums, std::move(Other.Enums));
    reduceSymbolIDs(I.Functions, std::move(Other.Functions));
    reduceSymbolIDs(I.Variables, std::move(Other.Variables));
    reduceSymbolIDs(I.Concepts, std::move(Other.Concepts));
    reduceSymbolIDs(I.Guides, std::move(Other.Guides));
    reduceSymbolIDs(I.Usings, std::move(Other.Usings));
}

void
merge(NamespaceInfo& I, NamespaceInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(dynamic_cast<Info&>(I), std::move(dynamic_cast<Info&>(Other)));
    merge(I.Members, std::move(Other.Members));
    reduceNames(I.UsingDirectives, std::move(Other.UsingDirectives));
    I.IsInline |= Other.IsInline;
    I.IsAnonymous |= Other.IsAnonymous;
}
} // clang::mrdocs

