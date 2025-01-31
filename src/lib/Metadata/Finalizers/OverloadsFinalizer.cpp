//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "OverloadsFinalizer.hpp"
#include "lib/Support/NameParser.hpp"

namespace clang::mrdocs {

void
OverloadsFinalizer::
foldRecordMembers(std::vector<SymbolID> const& ids)
{
    for (SymbolID const& id: ids)
    {
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* record = dynamic_cast<RecordInfo*>(info.get());
        MRDOCS_CHECK_OR_CONTINUE(record);
        operator()(*record);
    }
}

void
OverloadsFinalizer::
foldNamespaceMembers(std::vector<SymbolID> const& ids)
{
    for (SymbolID const& id: ids)
    {
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* ns = dynamic_cast<NamespaceInfo*>(info.get());
        MRDOCS_CHECK_OR_CONTINUE(ns);
        operator()(*ns);
    }
}

void
OverloadsFinalizer::
foldOverloads(SymbolID const& parent, std::vector<SymbolID>& ids)
{
    for (auto it = ids.begin(); it != ids.end(); ++it)
    {
        // Get the FunctionInfo for the current id
        auto infoIt = info_.find(*it);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* function = dynamic_cast<FunctionInfo*>(info.get());
        MRDOCS_CHECK_OR_CONTINUE(function);

        // Check if the FunctionInfo is unique
        auto sameNameIt =
            std::find_if(
                it + 1,
                ids.end(),
                [&](SymbolID const& otherID)
                {
                    auto const otherInfoIt = info_.find(otherID);
                    MRDOCS_CHECK_OR(otherInfoIt != info_.end(), false);
                    auto& otherInfo = *otherInfoIt;
                    return function->Name == otherInfo->Name;
                });
        if (sameNameIt == ids.end())
        {
            continue;
        }

        // FunctionInfo is not unique, so merge it with the other FunctionInfos
        // into an OverloadsInfo
        OverloadsInfo O(parent, function->Name);
        addMember(O, *function);
        *it = O.id;
        auto const itOffset = it - ids.begin();
        for (auto otherIt = it + 1; otherIt != ids.end(); ++otherIt)
        {
            auto otherInfoIt = info_.find(*otherIt);
            MRDOCS_CHECK_OR_CONTINUE(otherInfoIt != info_.end());
            auto& otherInfo = *otherInfoIt;
            auto* otherFunction = dynamic_cast<FunctionInfo*>(otherInfo.get());
            MRDOCS_CHECK_OR_CONTINUE(otherFunction);
            if (function->Name == otherFunction->Name)
            {
                addMember(O, *otherFunction);
                otherIt = std::prev(ids.erase(otherIt));
            }
        }
        it = ids.begin() + itOffset;
        info_.emplace(std::make_unique<OverloadsInfo>(std::move(O)));
    }
}

void
OverloadsFinalizer::
operator()(NamespaceInfo& I)
{
    foldOverloads(I.id, I.Members.Functions);
    foldRecordMembers(I.Members.Records);
    foldNamespaceMembers(I.Members.Namespaces);
}

void
OverloadsFinalizer::
operator()(RecordInfo& I)
{
    foldOverloads(I.id, I.Interface.Public.Functions);
    foldOverloads(I.id, I.Interface.Protected.Functions);
    foldOverloads(I.id, I.Interface.Private.Functions);
    foldOverloads(I.id, I.Interface.Public.StaticFunctions);
    foldOverloads(I.id, I.Interface.Protected.StaticFunctions);
    foldOverloads(I.id, I.Interface.Private.StaticFunctions);
    foldRecordMembers(I.Interface.Public.Records);
    foldRecordMembers(I.Interface.Protected.Records);
    foldRecordMembers(I.Interface.Private.Records);
}

} // clang::mrdocs
