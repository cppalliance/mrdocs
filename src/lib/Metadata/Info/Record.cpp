//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyObject.hpp>
#include <llvm/ADT/STLExtras.h>
#include <mrdocs/Metadata/Info/Record.hpp>

namespace clang::mrdocs {

dom::String
toString(
    RecordKeyKind kind) noexcept
{
    switch(kind)
    {
    case RecordKeyKind::Struct:
        return "struct";
    case RecordKeyKind::Class:
        return "class";
    case RecordKeyKind::Union:
        return "union";
    default:
        MRDOCS_UNREACHABLE();
    }
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
} // (anon)

std::strong_ordering
RecordInfo::
operator<=>(RecordInfo const& other) const
{
    if (auto const cmp = Name <=> other.Name;
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (auto const cmp = Template.operator bool() <=> other.Template.operator bool();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args.size() <=> other.Template->Args.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params.size() <=> other.Template->Params.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args <=> other.Template->Args;
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params <=> other.Template->Params;
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    return this->asInfo() <=> other.asInfo();
}

void
merge(RecordTranche& I, RecordTranche&& Other)
{
    reduceSymbolIDs(I.NamespaceAliases, std::move(Other.NamespaceAliases));
    reduceSymbolIDs(I.Typedefs, std::move(Other.Typedefs));
    reduceSymbolIDs(I.Records, std::move(Other.Records));
    reduceSymbolIDs(I.Enums, std::move(Other.Enums));
    reduceSymbolIDs(I.Functions, std::move(Other.Functions));
    reduceSymbolIDs(I.StaticFunctions, std::move(Other.StaticFunctions));
    reduceSymbolIDs(I.Variables, std::move(Other.Variables));
    reduceSymbolIDs(I.StaticVariables, std::move(Other.StaticVariables));
    reduceSymbolIDs(I.Concepts, std::move(Other.Concepts));
    reduceSymbolIDs(I.Guides, std::move(Other.Guides));
    reduceSymbolIDs(I.Friends, std::move(Other.Friends));
}

void
merge(RecordInterface& I, RecordInterface&& Other)
{
    merge(I.Public, std::move(Other.Public));
    merge(I.Protected, std::move(Other.Protected));
    merge(I.Private, std::move(Other.Private));
}

void
merge(RecordInfo& I, RecordInfo&& Other)
{
    merge(I.asInfo(), std::move(Other.asInfo()));
    if (Other.KeyKind != RecordKeyKind::Struct &&
        I.KeyKind != Other.KeyKind)
    {
        I.KeyKind = Other.KeyKind;
    }
    I.IsTypeDef |= Other.IsTypeDef;
    I.IsFinal |= Other.IsFinal;
    I.IsFinalDestructor |= Other.IsFinalDestructor;
    if (I.Bases.empty())
    {
        I.Bases = std::move(Other.Bases);
    }
    merge(I.Interface, std::move(Other.Interface));
    if (!I.Template)
    {
        I.Template = std::move(Other.Template);
    }
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    BaseInfo const& I,
    DomCorpus const* domCorpus)
{
    io.map("access", I.Access);
    io.map("isPublic", I.Access == AccessKind::Public);
    io.map("isProtected", I.Access == AccessKind::Protected);
    io.map("isPrivate", I.Access == AccessKind::Private);
    io.map("isVirtual", I.IsVirtual);
    io.map("type", dom::ValueFrom(I.Type, domCorpus));
    io.map("symbol", I.Type->namedSymbol());
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BaseInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs
