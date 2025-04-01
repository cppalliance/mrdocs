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

namespace clang::mrdocs {

void
OverloadsFinalizer::
foldRecordMembers(std::vector<SymbolID> const& ids)
{
    for (SymbolID const& id: ids)
    {
        Info* infoPtr = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr->isRecord());
        operator()(infoPtr->asRecord());
    }
}

void
OverloadsFinalizer::
foldNamespaceMembers(std::vector<SymbolID> const& namespaceIds)
{
    for (SymbolID const& namespaceId: namespaceIds)
    {
        Info* namespaceInfoPtr = corpus_.find(namespaceId);
        MRDOCS_CHECK_OR_CONTINUE(namespaceInfoPtr);
        MRDOCS_CHECK_OR_CONTINUE(namespaceInfoPtr->isNamespace());
        operator()(namespaceInfoPtr->asNamespace());
    }
}

namespace {
SymbolID
findBaseClassPermutation(
    SymbolID const& contextId,
    CorpusImpl& corpus,
    ArrayRef<SymbolID> sameNameFunctionIds)
{
    Info* info = corpus.find(contextId);
    MRDOCS_CHECK_OR(info, SymbolID::invalid);
    if (auto const* record = info->asRecordPtr())
    {
        for (auto const& base: record->Bases)
        {
            auto const baseInfo = corpus.find(base.Type->namedSymbol());
            MRDOCS_CHECK_OR_CONTINUE(baseInfo);
            auto const baseRecord = baseInfo->asRecordPtr();
            MRDOCS_CHECK_OR_CONTINUE(baseRecord);
            RecordTranche* tranchesPtrs[] = {
                &baseRecord->Interface.Public,
                &baseRecord->Interface.Protected,
                &baseRecord->Interface.Private,
            };
            for (RecordTranche* tranchePtr: tranchesPtrs)
            {
                std::vector<SymbolID>* trancheFunctionPtrs[] = {
                    &tranchePtr->Functions,
                    &tranchePtr->StaticFunctions
                };
                for (std::vector<SymbolID>* trancheFunctionsPtr:
                     trancheFunctionPtrs)
                {
                    for (SymbolID const& baseID: *trancheFunctionsPtr)
                    {
                        Info* baseFuncMember = corpus.find(baseID);
                        MRDOCS_CHECK_OR_CONTINUE(baseFuncMember);
                        MRDOCS_CHECK_OR_CONTINUE(baseFuncMember->isOverloads());
                        auto* overloads = baseFuncMember->asOverloadsPtr();
                        MRDOCS_CHECK_OR_CONTINUE(overloads);
                        // Does this overload set have the same functions
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::is_permutation(
                                overloads->Members,
                                sameNameFunctionIds));
                        return overloads->id;
                    }
                }
            }
        }
    }
    return SymbolID::invalid;
}
}

void
OverloadsFinalizer::
foldOverloads(SymbolID const& contextId, std::vector<SymbolID>& functionIds)
{
    for (auto functionIdIt = functionIds.begin();
         functionIdIt != functionIds.end();
         ++functionIdIt)
    {
        // Get the FunctionInfo for the current id
        auto recordInfoPtr = corpus_.find(*functionIdIt);
        MRDOCS_CHECK_OR_CONTINUE(recordInfoPtr);
        auto* function = recordInfoPtr->asFunctionPtr();
        MRDOCS_CHECK_OR_CONTINUE(function);

        // Check if the FunctionInfo is unique
        std::ranges::subrange otherFunctionIds(
            std::next(functionIdIt),
            functionIds.end());
        auto isSameNameFunction = [&](SymbolID const& otherID) {
            auto const otherFunctionPtr = corpus_.find(otherID);
            MRDOCS_CHECK_OR(otherFunctionPtr, false);
            Info const& otherInfo = *otherFunctionPtr;
            return function->Name == otherInfo.Name;
        };
        auto sameNameIt = std::ranges::
            find_if(otherFunctionIds, isSameNameFunction);
        bool const isUniqueFunction = sameNameIt == otherFunctionIds.end();
        MRDOCS_CHECK_OR_CONTINUE(!isUniqueFunction);

        // Create a list of FunctionInfo overloads
        auto sameNameFunctionIdsView =
            std::ranges::subrange(functionIdIt, functionIds.end()) |
            std::views::filter(isSameNameFunction);
        SmallVector<SymbolID, 16> sameNameFunctionIds(
            sameNameFunctionIdsView.begin(),
            sameNameFunctionIdsView.end());

        // Check if any of the base classes has an overload set
        // with the exact same function ids. If that's the case,
        // the function will create a reference.
        SymbolID equivalentOverloadsID = findBaseClassPermutation(
            contextId, corpus_, sameNameFunctionIds);
        if (equivalentOverloadsID)
        {
            MRDOCS_ASSERT(corpus_.find(equivalentOverloadsID));
            // This base overload set becomes the
            // representation in the record
            *functionIdIt = equivalentOverloadsID;
            auto const offset = functionIdIt - functionIds.begin();
            // Erase the other function ids with
            // the same name
            for (SymbolID sameNameId: sameNameFunctionIds)
            {
                std::erase(functionIds, sameNameId);
            }
            functionIdIt = functionIds.begin() + offset;
            continue;
        }

        // FunctionInfo is not unique and there's no equivalent
        // overload set in base classes, so we merge it with the
        // other FunctionInfos into a new OverloadsInfo
        OverloadsInfo O(contextId, function->Name);
        addMember(O, *function);
        *functionIdIt = O.id;
        auto const itOffset = functionIdIt - functionIds.begin();
        for (auto otherIt = functionIdIt + 1; otherIt != functionIds.end(); ++otherIt)
        {
            Info* otherInfoPtr = corpus_.find(*otherIt);
            MRDOCS_CHECK_OR_CONTINUE(otherInfoPtr);
            auto* otherFunction = otherInfoPtr->asFunctionPtr();
            MRDOCS_CHECK_OR_CONTINUE(otherFunction);
            if (function->Name == otherFunction->Name)
            {
                addMember(O, *otherFunction);
                otherIt = std::prev(functionIds.erase(otherIt));
            }
        }
        functionIdIt = functionIds.begin() + itOffset;
        corpus_.info_.emplace(std::make_unique<OverloadsInfo>(std::move(O)));
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
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    for (auto& b: I.Bases)
    {
        auto& BT = b.Type;
        MRDOCS_CHECK_OR(BT);
        MRDOCS_CHECK_OR(BT->isNamed());
        auto& NT = dynamic_cast<NamedTypeInfo&>(*BT);
        MRDOCS_CHECK_OR(NT.Name);
        auto& NI = dynamic_cast<NameInfo&>(*NT.Name);
        MRDOCS_CHECK_OR(NI.id);
        Info* baseInfo = corpus_.find(NI.id);
        MRDOCS_CHECK_OR(baseInfo);
        auto* baseRecord = baseInfo->asRecordPtr();
        MRDOCS_CHECK_OR(baseRecord);
        operator()(*baseRecord);
    }
    foldOverloads(I.id, I.Interface.Public.Functions);
    foldOverloads(I.id, I.Interface.Protected.Functions);
    foldOverloads(I.id, I.Interface.Private.Functions);
    foldOverloads(I.id, I.Interface.Public.StaticFunctions);
    foldOverloads(I.id, I.Interface.Protected.StaticFunctions);
    foldOverloads(I.id, I.Interface.Private.StaticFunctions);
    foldRecordMembers(I.Interface.Public.Records);
    foldRecordMembers(I.Interface.Protected.Records);
    foldRecordMembers(I.Interface.Private.Records);
    finalized_.emplace(I.id);
}

} // clang::mrdocs
