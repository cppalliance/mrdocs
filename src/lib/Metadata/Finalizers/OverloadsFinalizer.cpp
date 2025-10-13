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
#include <mrdocs/Support/Assert.hpp>

namespace clang::mrdocs {

namespace {
SymbolID
findBaseClassPermutation(
    SymbolID const& contextId,
    CorpusImpl& corpus,
    ArrayRef<SymbolID> sameNameFunctionIds)
{
    // Find the RecordInfo
    Info* info = corpus.find(contextId);
    MRDOCS_CHECK_OR(info, SymbolID::invalid);
    MRDOCS_CHECK_OR(info->isRecord(), SymbolID::invalid);
    for (auto const& base: info->asRecordPtr()->Bases)
    {
        // Find the i-th base class
        auto const baseInfo = corpus.find(base.Type->namedSymbol());
        MRDOCS_CHECK_OR_CONTINUE(baseInfo);
        auto const baseRecord = baseInfo->asRecordPtr();
        MRDOCS_CHECK_OR_CONTINUE(baseRecord);

        // Find an overload set that's a permutation of the same
        // name functions
        for (auto const& baseMember: baseRecord->Members)
        {
            MRDOCS_CHECK_OR_CONTINUE(baseMember.Kind == InfoKind::Function);

            SymbolID const& baseID = baseMember.id;
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
    return SymbolID::invalid;
}

SymbolID
findIntroducedNamespacePermutation(
    SymbolID const& contextId,
    CorpusImpl& corpus,
    ArrayRef<SymbolID> sameNameFunctionIds)
{
    // Find the UsingInfo
    Info* info = corpus.find(contextId);
    MRDOCS_CHECK_OR(info, SymbolID::invalid);
    MRDOCS_CHECK_OR(info->isUsing(), SymbolID::invalid);

    // Find the FunctionInfo for the first shadow declaration
    MRDOCS_CHECK_OR(!sameNameFunctionIds.empty(), SymbolID::invalid);
    Info* firstShadowInfo = corpus.find(sameNameFunctionIds.front());
    MRDOCS_CHECK_OR(firstShadowInfo, SymbolID::invalid);
    MRDOCS_CHECK_OR(firstShadowInfo->isFunction(), SymbolID::invalid);
    auto* firstShadowFunction = firstShadowInfo->asFunctionPtr();

    // Find the introduced namespace of the first shadow declaration
    MRDOCS_CHECK_OR(firstShadowFunction->Parent, SymbolID::invalid);
    Info* parentInfo = corpus.find(firstShadowFunction->Parent);
    MRDOCS_CHECK_OR(parentInfo, SymbolID::invalid);
    MRDOCS_CHECK_OR(parentInfo->isNamespace(), SymbolID::invalid);
    auto* parentNamespace = parentInfo->asNamespacePtr();

    // Find an overload set that's a permutation of the same name functions
    for (SymbolID const& baseID: parentNamespace->Members.Functions)
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
    return SymbolID::invalid;
}
}

void
OverloadsFinalizer::
foldOverloads(SymbolID const& contextId, std::vector<SymbolID>& functionIds, AccessKind access, bool isStatic)
{
    Info* contextInfo = corpus_.find(contextId);
    MRDOCS_CHECK_OR(contextInfo);

    for (auto functionIdIt = functionIds.begin();
         functionIdIt != functionIds.end();
         ++functionIdIt)
    {
        // Get the FunctionInfo for the current id
        auto infoPtr = corpus_.find(*functionIdIt);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* function = infoPtr->asFunctionPtr();
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
        if (contextInfo->isRecord())
        {
            SymbolID equivalentOverloadsID = findBaseClassPermutation(
                contextId,
                corpus_,
                sameNameFunctionIds);
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
        }

        // Check if the namespace of the name introduced in the
        // using declaration has an overload set with the
        // exact same function ids. If that's the case,
        // the function will create a reference.
        if (contextInfo->isUsing())
        {
            SymbolID introducedOverloadsID = findIntroducedNamespacePermutation(
                contextId,
                corpus_,
                sameNameFunctionIds);
            if (introducedOverloadsID)
            {
                MRDOCS_ASSERT(corpus_.find(introducedOverloadsID));
                // This base overload set becomes the
                // representation in the record
                *functionIdIt = introducedOverloadsID;
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
        }

        // FunctionInfo is not unique and there's no equivalent
        // overload set in base classes, so we merge it with the
        // other FunctionInfos into a new OverloadsInfo
        OverloadsInfo O(contextId, function->Name, function->Access, isStatic);
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
        MRDOCS_ASSERT(corpus_.info_.emplace(std::make_unique<OverloadsInfo>(std::move(O))).second);
    }
}

// FIXME: dedup with above
void
OverloadsFinalizer::
foldOverloads(SymbolID const& contextId, std::vector<MemberInfo>& members)
{
    Info* contextInfo = corpus_.find(contextId);
    MRDOCS_CHECK_OR(contextInfo);

    for (auto memberIt = members.begin();
         memberIt != members.end();
         ++memberIt)
    {
        MRDOCS_CHECK_OR_CONTINUE(memberIt->Kind == InfoKind::Function);

        // Get the FunctionInfo for the current id
        auto infoPtr = corpus_.find(memberIt->id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* function = infoPtr->asFunctionPtr();
        MRDOCS_CHECK_OR_CONTINUE(function);

        AccessKind const access = function->Access;
        bool const isStatic = (function->StorageClass == StorageClassKind::Static);

        // Check if the FunctionInfo is unique
        std::ranges::subrange otherMembers(
            std::next(memberIt),
            members.end());
        auto isSameNameFunction = [&](MemberInfo const& other) {
            MRDOCS_CHECK_OR(other.EffectiveAccess == access, false);
            MRDOCS_CHECK_OR(other.Kind == InfoKind::Function, false);
            SymbolID const& otherID = other.id;
            auto const otherFunctionPtr = corpus_.find(otherID);
            MRDOCS_CHECK_OR(otherFunctionPtr, false);
            FunctionInfo const& otherInfo = otherFunctionPtr->asFunction();
            return function->Name == otherInfo.Name && (isStatic == (otherInfo.StorageClass == StorageClassKind::Static)) ;
        };
        auto sameNameIt = std::ranges::
            find_if(otherMembers, isSameNameFunction);
        bool const isUniqueFunction = sameNameIt == otherMembers.end();
        MRDOCS_CHECK_OR_CONTINUE(!isUniqueFunction);

        // Create a list of FunctionInfo overloads
        auto sameNameMembersView =
            std::ranges::subrange(memberIt, members.end()) |
            std::views::filter(isSameNameFunction) |
            std::views::transform(&MemberInfo::id);
        SmallVector<SymbolID, 16> sameNameMembers(
            sameNameMembersView.begin(),
            sameNameMembersView.end());

        // Check if any of the base classes has an overload set
        // with the exact same function ids. If that's the case,
        // the function will create a reference.
        if (contextInfo->isRecord())
        {
            SymbolID equivalentOverloadsID = findBaseClassPermutation(
                contextId,
                corpus_,
                sameNameMembers);
            if (equivalentOverloadsID)
            {
                MRDOCS_ASSERT(corpus_.find(equivalentOverloadsID));
                // This base overload set becomes the
                // representation in the record
                memberIt->id = equivalentOverloadsID;
                auto const offset = memberIt - members.begin();
                // Erase the other function ids with
                // the same name
                for (SymbolID sameNameId: sameNameMembers)
                {
                    std::erase_if(members, [&](MemberInfo const& member) { return member.id == sameNameId; });
                }
                memberIt = members.begin() + offset;
                continue;
            }
        }

        // Check if the namespace of the name introduced in the
        // using declaration has an overload set with the
        // exact same function ids. If that's the case,
        // the function will create a reference.
        if (contextInfo->isUsing())
        {
            SymbolID introducedOverloadsID = findIntroducedNamespacePermutation(
                contextId,
                corpus_,
                sameNameMembers);
            if (introducedOverloadsID)
            {
                MRDOCS_ASSERT(corpus_.find(introducedOverloadsID));
                // This base overload set becomes the
                // representation in the record
                memberIt->id = introducedOverloadsID;
                auto const offset = memberIt - members.begin();
                // Erase the other function ids with
                // the same name
                for (SymbolID sameNameId: sameNameMembers)
                {
                    std::erase_if(members, [&](MemberInfo const& member) { return member.id == sameNameId; });
                }
                memberIt = members.begin() + offset;
                continue;
            }
        }

        // FunctionInfo is not unique and there's no equivalent
        // overload set in base classes, so we merge it with the
        // other FunctionInfos into a new OverloadsInfo
        OverloadsInfo O(contextId, function->Name, access, isStatic);
        addMember(O, *function);
        memberIt->id = O.id;
        auto const itOffset = memberIt - members.begin();
        for (auto otherIt = memberIt + 1; otherIt != members.end(); ++otherIt)
        {
            MRDOCS_CHECK_OR_CONTINUE(otherIt->EffectiveAccess == access);
            MRDOCS_CHECK_OR_CONTINUE(otherIt->Kind == InfoKind::Function);

            Info* otherInfoPtr = corpus_.find(otherIt->id);
            MRDOCS_CHECK_OR_CONTINUE(otherInfoPtr);
            auto* otherFunction = otherInfoPtr->asFunctionPtr();
            MRDOCS_CHECK_OR_CONTINUE(otherFunction);
            MRDOCS_CHECK_OR_CONTINUE(isStatic == (otherFunction->StorageClass == StorageClassKind::Static));
            if (function->Name == otherFunction->Name)
            {
                addMember(O, *otherFunction);
                otherIt = std::prev(members.erase(otherIt));
            }
        }
        memberIt = members.begin() + itOffset;
        MRDOCS_ASSERT(corpus_.info_.emplace(std::make_unique<OverloadsInfo>(std::move(O))).second);
    }
}

namespace {
template <class T, range_of<SymbolID> R>
constexpr
auto
toDerivedView(R&& ids, CorpusImpl& c)
{
    return ids |
       std::views::transform([&c](SymbolID const& id) {
            return c.find(id);
        }) |
       std::views::filter([](Info* infoPtr) {
            return infoPtr != nullptr;
       }) |
      std::views::transform([](Info* infoPtr) -> T* {
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
OverloadsFinalizer::
operator()(NamespaceInfo& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    finalized_.emplace(I.id);

    foldOverloads(I.id, I.Members.Functions, AccessKind::None, true);
    for (RecordInfo& RI: toDerivedView<RecordInfo>(I.Members.Records, corpus_))
    {
        operator()(RI);
    }
    for (NamespaceInfo& NI: toDerivedView<NamespaceInfo>(I.Members.Namespaces, corpus_))
    {
        operator()(NI);
    }
    for (UsingInfo& UI: toDerivedView<UsingInfo>(I.Members.Usings, corpus_))
    {
        operator()(UI);
    }
}

void
OverloadsFinalizer::
operator()(RecordInfo& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    finalized_.emplace(I.id);

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

    foldOverloads(I.id, I.Members);

    auto&& memberIds = std::ranges::views::transform(I.Members, &MemberInfo::id);
    for (RecordInfo& RI: toDerivedView<RecordInfo>(memberIds, corpus_)) {
        operator()(RI);
    }
}

void
OverloadsFinalizer::
operator()(UsingInfo& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    finalized_.emplace(I.id);

    auto shadowFunctions = toDerivedView<FunctionInfo>(I.ShadowDeclarations, corpus_);
    for (FunctionInfo& FI: shadowFunctions)
    {
        Info* PI = corpus_.find(FI.Parent);
        MRDOCS_CHECK_OR_CONTINUE(PI);
        if (auto* NI = PI->asNamespacePtr())
        {
            operator()(*NI);
        }
        else if (auto* RI = PI->asRecordPtr())
        {
            operator()(*RI);
        }
        break;
    }
    foldOverloads(I.id, I.ShadowDeclarations, AccessKind::None, true);
}

} // clang::mrdocs
