//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "SortMembersFinalizer.hpp"
#include "SymbolIDCompare.hpp"
#include <algorithm>
#include <ranges>

namespace mrdocs {

void
SortMembersFinalizer::
sortMembers(std::vector<SymbolID>& ids)
{
    SymbolIDCompareFn const pred{corpus_, config_};
    std::ranges::sort(ids, pred);
}

void
SortMembersFinalizer::
sortMembers(NamespaceTranche& T)
{
    // Sort every member bucket. The lists are discovered by reflection,
    // so adding a member to NamespaceTranche (e.g. Macros) is sorted
    // automatically, with no hand-maintained list to keep in sync.
    describe::for_each_member<NamespaceTranche>([&](auto const d) {
        sortMembers(T.*d.pointer);
    });
}

void
SortMembersFinalizer::
sortMembers(RecordTranche& T)
{
    describe::for_each_member<RecordTranche>([&](auto const d) {
        sortMembers(T.*d.pointer);
    });
}

void
SortMembersFinalizer::
sortMembers(RecordInterface& I)
{
    sortMembers(I.Public);
    sortMembers(I.Protected);
    sortMembers(I.Private);
}

namespace {
template <class T>
constexpr
auto
toDerivedView(std::vector<SymbolID> const& ids, Corpus& c)
{
    return ids |
       std::views::transform([&c](SymbolID const& id) {
            return c.find(id);
        }) |
       std::views::filter([](Symbol* infoPtr) {
            return infoPtr != nullptr;
       }) |
      std::views::transform([](Symbol* infoPtr) -> T* {
         return dynamic_cast<T*>(infoPtr);
      }) |
      std::views::filter([](T* ptr) {
         return ptr != nullptr;
      }) |
      std::views::transform([](T* ptr) -> T& {
         return *ptr;
      });
}
}

void
SortMembersFinalizer::
operator()(NamespaceSymbol& I)
{
    // Sort members of all tranches
    sortMembers(I.Members);

    // Recursively sort members of child namespaces, records, and overloads
    for (RecordSymbol& RI: toDerivedView<RecordSymbol>(I.Members.Records, corpus_))
    {
        operator()(RI);
    }
    for (NamespaceSymbol& RI: toDerivedView<NamespaceSymbol>(I.Members.Namespaces, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Members.Functions, corpus_))
    {
        operator()(RI);
    }
}

void
SortMembersFinalizer::
operator()(RecordSymbol& I)
{
    // Sort members of all tranches if sorting is enabled for records
    if (config_.sortMembers)
    {
        sortMembers(I.Interface);
    }

    // Recursively sort members of child records and overloads
    for (RecordSymbol& RI: toDerivedView<RecordSymbol>(I.Interface.Public.Records, corpus_))
    {
        operator()(RI);
    }
    for (RecordSymbol& RI: toDerivedView<RecordSymbol>(I.Interface.Protected.Records, corpus_))
    {
        operator()(RI);
    }
    for (RecordSymbol& RI: toDerivedView<RecordSymbol>(I.Interface.Private.Records, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Interface.Public.Functions, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Interface.Protected.Functions, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Interface.Private.Functions, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Interface.Public.StaticFunctions, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Interface.Protected.StaticFunctions, corpus_))
    {
        operator()(RI);
    }
    for (OverloadsSymbol& RI: toDerivedView<OverloadsSymbol>(I.Interface.Private.StaticFunctions, corpus_))
    {
        operator()(RI);
    }
}

void
SortMembersFinalizer::
operator()(OverloadsSymbol& I)
{
    // Sort the member functions
    sortMembers(I.Members);
}

} // mrdocs
