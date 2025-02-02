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
#include "lib/Support/NameParser.hpp"
#include <ranges>
#include <algorithm>

namespace clang::mrdocs {

namespace {
// Comparison function for symbol IDs
struct SymbolIDCompareFn
{
    InfoSet& info_;
    Config const& config_;

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
        auto const& lhsInfoIt = info_.find(lhsId);
        MRDOCS_CHECK_OR(lhsInfoIt != info_.end(), false);
        auto const& rhsInfoIt = info_.find(rhsId);
        MRDOCS_CHECK_OR(rhsInfoIt != info_.end(), true);
        auto& lhsPtr = *lhsInfoIt;
        MRDOCS_CHECK_OR(lhsPtr, false);
        auto& rhsPtr = *rhsInfoIt;
        MRDOCS_CHECK_OR(rhsPtr, true);
        Info const& lhs = **lhsInfoIt;
        Info const& rhs = **rhsInfoIt;

        std::optional<FunctionClass> const lhsClass = findFunctionClass(lhs);
        std::optional<FunctionClass> const rhsClass = findFunctionClass(rhs);
        if (config_->sortMembersCtors1St)
        {
            bool const lhsIsCtor = lhsClass && *lhsClass == FunctionClass::Constructor;
            bool const rhsIsCtor = rhsClass && *rhsClass == FunctionClass::Constructor;
            if (lhsIsCtor != rhsIsCtor)
            {
                return lhsIsCtor;
            }
        }

        if (config_->sortMembersDtors1St)
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
        if (config_->sortMembersAssignment1St)
        {
            bool const lhsIsAssign = lhsOp && *lhsOp == OperatorKind::Equal;
            bool const rhsIsAssign = rhsOp && *rhsOp == OperatorKind::Equal;
            if (lhsIsAssign != rhsIsAssign)
            {
                return lhsIsAssign;
            }
        }

        if (config_->sortMembersRelationalLast)
        {
            bool const lhsIsRel = lhsOp && (
                *lhsOp == OperatorKind::Exclaim ||
                *lhsOp == OperatorKind::EqualEqual ||
                *lhsOp == OperatorKind::ExclaimEqual ||
                *lhsOp == OperatorKind::Less ||
                *lhsOp == OperatorKind::Greater ||
                *lhsOp == OperatorKind::LessEqual ||
                *lhsOp == OperatorKind::GreaterEqual ||
                *lhsOp == OperatorKind::Spaceship);
            bool const rhsIsRel = rhsOp && (
                *rhsOp == OperatorKind::Exclaim ||
                *rhsOp == OperatorKind::EqualEqual ||
                *rhsOp == OperatorKind::ExclaimEqual ||
                *rhsOp == OperatorKind::Less ||
                *rhsOp == OperatorKind::Greater ||
                *rhsOp == OperatorKind::LessEqual ||
                *rhsOp == OperatorKind::GreaterEqual ||
                *rhsOp == OperatorKind::Spaceship);
            if (lhsIsRel != rhsIsRel)
            {
                return !lhsIsRel;
            }
            if (lhsIsRel && rhsIsRel)
            {
                return std::is_lt(*lhsOp <=> *rhsOp);
            }
        }

        if (config_->sortMembersConversionLast)
        {
            bool const lhsIsConvertion = lhsClass && *lhsClass == FunctionClass::Conversion;
            bool const rhsIsConvertion = rhsClass && *rhsClass == FunctionClass::Conversion;
            if (lhsIsConvertion != rhsIsConvertion)
            {
                return !lhsIsConvertion;
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
    SymbolIDCompareFn const pred{info_, config_};
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
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* ns = dynamic_cast<NamespaceInfo*>(info.get());
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
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* record = dynamic_cast<RecordInfo*>(info.get());
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
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* overloads = dynamic_cast<OverloadsInfo*>(info.get());
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
