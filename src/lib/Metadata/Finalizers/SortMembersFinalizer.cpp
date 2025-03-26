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
#include <ranges>
#include <algorithm>

namespace clang::mrdocs {

namespace {
// Comparison function by symbol IDs
struct SymbolIDCompareFn
{
    CorpusImpl const& corpus_;

    template <class InfoTy>
    static
    std::optional<FunctionClass>
    findFunctionClass(InfoTy const& I)
    {
        if constexpr (std::same_as<InfoTy, Info>)
        {
            return visit(I, []<class U>(U const& u)
                -> std::optional<FunctionClass>
            {
                return findFunctionClass<U>(u);
            });
        }
        else if constexpr (
            std::same_as<FunctionInfo, InfoTy> ||
            std::same_as<OverloadsInfo, InfoTy>)
        {
            return I.Class;
        }
        return std::nullopt;
    }

    template <class InfoTy>
    static
    std::optional<OperatorKind>
    findOperatorKind(InfoTy const& I)
    {
        if constexpr (std::same_as<InfoTy, Info>)
        {
            return visit(I, []<class U>(U const& u)
                -> std::optional<OperatorKind>
            {
                return findOperatorKind<U>(u);
            });
        }
        else if constexpr (
            std::same_as<FunctionInfo, InfoTy> ||
            std::same_as<OverloadsInfo, InfoTy>)
        {
            return I.OverloadedOperator;
        }
        return std::nullopt;
    }

    bool
    operator()(SymbolID const& lhsId, SymbolID const& rhsId) const
    {
        // Get Info from SymbolID
        Info const* lhsPtr = corpus_.find(lhsId);
        MRDOCS_CHECK_OR(lhsPtr, false);
        Info const* rhsPtr = corpus_.find(rhsId);
        MRDOCS_CHECK_OR(rhsPtr, true);
        Info const& lhs = *lhsPtr;
        Info const& rhs = *rhsPtr;

        std::optional<FunctionClass> const lhsClass = findFunctionClass(lhs);
        std::optional<FunctionClass> const rhsClass = findFunctionClass(rhs);
        if (corpus_.config->sortMembersCtors1St)
        {
            bool const lhsIsCtor = lhsClass && *lhsClass == FunctionClass::Constructor;
            bool const rhsIsCtor = rhsClass && *rhsClass == FunctionClass::Constructor;
            if (lhsIsCtor != rhsIsCtor)
            {
                return lhsIsCtor;
            }
        }

        if (corpus_.config->sortMembersDtors1St)
        {
            bool const lhsIsDtor = lhsClass && *lhsClass == FunctionClass::Destructor;
            bool const rhsIsDtor = rhsClass && *rhsClass == FunctionClass::Destructor;
            if (lhsIsDtor != rhsIsDtor)
            {
                return lhsIsDtor;
            }
        }

        std::optional<OperatorKind> const lhsOp = findOperatorKind(lhs);
        std::optional<OperatorKind> const rhsOp = findOperatorKind(rhs);
        if (corpus_.config->sortMembersAssignment1St)
        {
            bool const lhsIsAssign = lhsOp && *lhsOp == OperatorKind::Equal;
            bool const rhsIsAssign = rhsOp && *rhsOp == OperatorKind::Equal;
            if (lhsIsAssign != rhsIsAssign)
            {
                return lhsIsAssign;
            }
        }

        if (corpus_.config->sortMembersRelationalLast)
        {
            bool const lhsIsRel = lhsOp && (
                *lhsOp == OperatorKind::Exclaim ||
                *lhsOp == OperatorKind::EqualEqual ||
                *lhsOp == OperatorKind::ExclaimEqual ||
                *lhsOp == OperatorKind::Less ||
                *lhsOp == OperatorKind::Greater ||
                *lhsOp == OperatorKind::LessEqual ||
                *lhsOp == OperatorKind::GreaterEqual ||
                *lhsOp == OperatorKind::Spaceship ||
                *lhsOp == OperatorKind::LessLess);
            bool const rhsIsRel = rhsOp && (
                *rhsOp == OperatorKind::Exclaim ||
                *rhsOp == OperatorKind::EqualEqual ||
                *rhsOp == OperatorKind::ExclaimEqual ||
                *rhsOp == OperatorKind::Less ||
                *rhsOp == OperatorKind::Greater ||
                *rhsOp == OperatorKind::LessEqual ||
                *rhsOp == OperatorKind::GreaterEqual ||
                *rhsOp == OperatorKind::Spaceship ||
                *rhsOp == OperatorKind::LessLess);
            if (lhsIsRel != rhsIsRel)
            {
                return !lhsIsRel;
            }
            if (lhsIsRel && rhsIsRel)
            {
                return std::is_lt(*lhsOp <=> *rhsOp);
            }
        }

        if (corpus_.config->sortMembersConversionLast)
        {
            bool const lhsIsConvertion = lhsClass && *lhsClass == FunctionClass::Conversion;
            bool const rhsIsConvertion = rhsClass && *rhsClass == FunctionClass::Conversion;
            if (lhsIsConvertion != rhsIsConvertion)
            {
                return !lhsIsConvertion;
            }
        }

        // If both are constructors/assignment with 1 parameter, the copy/move
        // constructors come first
        if ((lhsClass && *lhsClass == FunctionClass::Constructor &&
            rhsClass && *rhsClass == FunctionClass::Constructor) ||
            (lhsOp && *lhsOp == OperatorKind::Equal &&
             rhsOp && *rhsOp == OperatorKind::Equal))
        {
            auto& lhsF = dynamic_cast<FunctionInfo const&>(lhs);
            auto& rhsF = dynamic_cast<FunctionInfo const&>(rhs);
            if (lhsF.Params.size() == 1 && rhsF.Params.size() == 1)
            {
                auto isCopyOrMoveConstOrAssign = [](FunctionInfo const& I) {
                    if (I.Params.size() == 1)
                    {
                        auto const& param = I.Params[0];
                        auto const& paramType = param.Type;
                        if (!paramType->isLValueReference()
                            && !paramType->isRValueReference())
                        {
                            return false;
                        }
                        auto const& paramRefPointee
                            = paramType->isLValueReference() ?
                                  get<LValueReferenceTypeInfo const&>(paramType)
                                      .PointeeType :
                                  get<RValueReferenceTypeInfo const&>(paramType)
                                      .PointeeType;
                        if (!paramRefPointee->isNamed())
                        {
                            return false;
                        }
                        return paramRefPointee->namedSymbol() == I.Parent;
                    }
                    return false;
                };

                bool const lhsIsCopyOrMove = isCopyOrMoveConstOrAssign(lhsF);
                bool const rhsIsCopyOrMove = isCopyOrMoveConstOrAssign(rhsF);
                if (auto const cmp = lhsIsCopyOrMove <=> rhsIsCopyOrMove;
                    cmp != 0)
                {
                    return !std::is_lt(cmp);
                }
                // Ensure move comes after copy
                if (lhsIsCopyOrMove && rhsIsCopyOrMove)
                {
                    bool const lhsIsMove = lhsF.Params[0].Type->isRValueReference();
                    bool const rhsIsMove = rhsF.Params[0].Type->isRValueReference();
                    if (lhsIsMove != rhsIsMove)
                    {
                        return !lhsIsMove;
                    }
                }
            }
        }

        if (auto const cmp = lhs.Name <=> rhs.Name; cmp != 0)
        {
            return std::is_lt(cmp);
        }

        return std::is_lt(CompareDerived(lhs, rhs));
    }
};
} // (anonymous)

void
SortMembersFinalizer::
sortMembers(std::vector<SymbolID>& ids)
{
    SymbolIDCompareFn const pred{corpus_};
    std::ranges::sort(ids, pred);
}

void
SortMembersFinalizer::
sortMembers(NamespaceTranche& T)
{
    sortMembers(T.Namespaces);
    sortMembers(T.NamespaceAliases);
    sortMembers(T.Typedefs);
    sortMembers(T.Records);
    sortMembers(T.Enums);
    sortMembers(T.Functions);
    sortMembers(T.Variables);
    sortMembers(T.Concepts);
    sortMembers(T.Guides);
    sortMembers(T.Usings);
}

void
SortMembersFinalizer::
sortMembers(RecordTranche& T)
{
    sortMembers(T.NamespaceAliases);
    sortMembers(T.Typedefs);
    sortMembers(T.Records);
    sortMembers(T.Enums);
    sortMembers(T.Functions);
    sortMembers(T.StaticFunctions);
    sortMembers(T.Variables);
    sortMembers(T.StaticVariables);
    sortMembers(T.Concepts);
    sortMembers(T.Guides);
    sortMembers(T.Friends);
    sortMembers(T.Usings);
}

void
SortMembersFinalizer::
sortMembers(RecordInterface& I)
{
    sortMembers(I.Public);
    sortMembers(I.Protected);
    sortMembers(I.Private);
}

void
SortMembersFinalizer::
sortNamespaceMembers(std::vector<SymbolID>& ids)
{
    for (SymbolID const& id: ids)
    {
        auto infoPtr = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* ns = dynamic_cast<NamespaceInfo*>(infoPtr);
        MRDOCS_CHECK_OR_CONTINUE(ns);
        operator()(*ns);
    }
}

void
SortMembersFinalizer::
sortRecordMembers(std::vector<SymbolID>& ids)
{
    for (SymbolID const& id: ids)
    {
        auto infoPtr = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* record = dynamic_cast<RecordInfo*>(infoPtr);
        MRDOCS_CHECK_OR_CONTINUE(record);
        operator()(*record);
    }
}

void
SortMembersFinalizer::
sortOverloadMembers(std::vector<SymbolID>& ids)
{
    for (SymbolID const& id: ids)
    {
        auto infoPtr = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* overloads = dynamic_cast<OverloadsInfo*>(infoPtr);
        MRDOCS_CHECK_OR_CONTINUE(overloads);
        operator()(*overloads);
    }
}

void
SortMembersFinalizer::
operator()(NamespaceInfo& I)
{
    sortMembers(I.Members);
    sortRecordMembers(I.Members.Records);
    sortNamespaceMembers(I.Members.Namespaces);
    sortOverloadMembers(I.Members.Functions);
}

void
SortMembersFinalizer::
operator()(RecordInfo& I)
{
    sortMembers(I.Interface);
    sortRecordMembers(I.Interface.Public.Records);
    sortRecordMembers(I.Interface.Protected.Records);
    sortRecordMembers(I.Interface.Private.Records);
    sortOverloadMembers(I.Interface.Public.Functions);
    sortOverloadMembers(I.Interface.Protected.Functions);
    sortOverloadMembers(I.Interface.Private.Functions);
    sortOverloadMembers(I.Interface.Public.StaticFunctions);
    sortOverloadMembers(I.Interface.Protected.StaticFunctions);
    sortOverloadMembers(I.Interface.Private.StaticFunctions);
}

void
SortMembersFinalizer::
operator()(OverloadsInfo& I)
{
    sortMembers(I.Members);
}

} // clang::mrdocs
