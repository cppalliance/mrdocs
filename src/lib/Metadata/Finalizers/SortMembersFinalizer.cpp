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
#include <algorithm>
#include <ranges>

namespace mrdocs {

namespace {
// Comparison function by symbol IDs
struct SymbolIDCompareFn
{
    CorpusImpl const& corpus_;

    template <class InfoTy>
    static
    Optional<FunctionClass>
    findFunctionClass(InfoTy const& I)
    {
        if constexpr (std::same_as<InfoTy, Symbol>)
        {
            return visit(I, []<class U>(U const& u)
                -> Optional<FunctionClass>
            {
                return findFunctionClass<U>(u);
            });
        }
        else if constexpr (
            std::same_as<FunctionSymbol, InfoTy> ||
            std::same_as<OverloadsSymbol, InfoTy>)
        {
            return I.Class;
        }
        return std::nullopt;
    }

    template <class InfoTy>
    static
    Optional<OperatorKind>
    findOperatorKind(InfoTy const& I)
    {
        if constexpr (std::same_as<InfoTy, Symbol>)
        {
            return visit(I, []<class U>(U const& u)
                -> Optional<OperatorKind>
            {
                return findOperatorKind<U>(u);
            });
        }
        else if constexpr (
            std::same_as<FunctionSymbol, InfoTy> ||
            std::same_as<OverloadsSymbol, InfoTy>)
        {
            return I.OverloadedOperator;
        }
        return std::nullopt;
    }

    bool
    operator()(SymbolID const& lhsId, SymbolID const& rhsId) const
    {
        // Get Info from SymbolID
        Symbol const* lhsPtr = corpus_.find(lhsId);
        MRDOCS_CHECK_OR(lhsPtr, false);
        Symbol const* rhsPtr = corpus_.find(rhsId);
        MRDOCS_CHECK_OR(rhsPtr, true);
        Symbol const& lhs = *lhsPtr;
        Symbol const& rhs = *rhsPtr;

        // Constructors come first
        Optional<FunctionClass> const lhsClass = findFunctionClass(lhs);
        Optional<FunctionClass> const rhsClass = findFunctionClass(rhs);
        if (corpus_.config->sortMembersCtors1St)
        {
            bool const lhsIsCtor = lhsClass && *lhsClass == FunctionClass::Constructor;
            bool const rhsIsCtor = rhsClass && *rhsClass == FunctionClass::Constructor;
            if (lhsIsCtor != rhsIsCtor)
            {
                return lhsIsCtor;
            }
        }

        // Destructors come next
        if (corpus_.config->sortMembersDtors1St)
        {
            bool const lhsIsDtor = lhsClass && *lhsClass == FunctionClass::Destructor;
            bool const rhsIsDtor = rhsClass && *rhsClass == FunctionClass::Destructor;
            if (lhsIsDtor != rhsIsDtor)
            {
                return lhsIsDtor;
            }
        }

        // Assignment operators come next
        Optional<OperatorKind> const lhsOp = findOperatorKind(lhs);
        Optional<OperatorKind> const rhsOp = findOperatorKind(rhs);
        if (corpus_.config->sortMembersAssignment1St)
        {
            bool const lhsIsAssign = lhsOp && *lhsOp == OperatorKind::Equal;
            bool const rhsIsAssign = rhsOp && *rhsOp == OperatorKind::Equal;
            if (lhsIsAssign != rhsIsAssign)
            {
                return lhsIsAssign;
            }
        }

        // Relational operators come last
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

        // Conversion operators come last
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
            FunctionSymbol const& lhsF = lhs.asFunction();
            FunctionSymbol const& rhsF = rhs.asFunction();
            if (lhsF.Params.size() == 1 && rhsF.Params.size() == 1)
            {
                auto isCopyOrMoveConstOrAssign = [](FunctionSymbol const& I) {
                    if (I.Params.size() == 1)
                    {
                        auto const& param = I.Params[0];
                        Polymorphic<Type> const& paramType = param.Type;
                        MRDOCS_ASSERT(!paramType.valueless_after_move());
                        MRDOCS_CHECK_OR(
                            paramType->isLValueReference() ||
                            paramType->isRValueReference(), false);
                        Polymorphic<Type> const &paramRefPointeeOpt =
                            paramType->isLValueReference()
                                ? paramType->asLValueReference().PointeeType
                                : paramType->asRValueReference().PointeeType;
                        MRDOCS_CHECK_OR(paramRefPointeeOpt, false);
                        auto const& paramRefPointee = *paramRefPointeeOpt;
                        if (!paramRefPointee.isNamed())
                        {
                            return false;
                        }
                        return paramRefPointee.namedSymbol() == I.Parent;
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
                    MRDOCS_ASSERT(!lhsF.Params[0].Type.valueless_after_move());
                    MRDOCS_ASSERT(!rhsF.Params[0].Type.valueless_after_move());
                    bool const lhsIsMove = lhsF.Params[0].Type->isRValueReference();
                    bool const rhsIsMove = rhsF.Params[0].Type->isRValueReference();
                    if (lhsIsMove != rhsIsMove)
                    {
                        return !lhsIsMove;
                    }
                }
            }
        }

        // Special cases are handled, so use the configuration criteria
        Symbol const* P = corpus_.find(lhs.Parent);
        bool const isClassMember = P && P->isRecord();
        auto const generalSortCriteria =
            isClassMember
                ? corpus_.config->sortMembersBy
                : corpus_.config->sortNamespaceMembersBy;
        switch (generalSortCriteria)
        {
        case mrdocs::PublicSettings::SortSymbolBy::Name:
            if (auto const cmp = lhs.Name <=> rhs.Name; cmp != 0)
            {
                return std::is_lt(cmp);
            }
            break;
        case mrdocs::PublicSettings::SortSymbolBy::Location:
        {
            // By location: short path, line, column
            auto const& lhsLoc = getPrimaryLocation(lhs);
            auto const& rhsLoc = getPrimaryLocation(rhs);
            if (auto const cmp = lhsLoc->ShortPath <=> rhsLoc->ShortPath;
                cmp != 0)
            {
                return std::is_lt(cmp);
            }
            if (auto const cmp = lhsLoc->LineNumber <=> rhsLoc->LineNumber;
                cmp != 0)
            {
                return std::is_lt(cmp);
            }
            break;
        }
        default:
            MRDOCS_UNREACHABLE();
        }

        // In case of a tie, we use the internal criteria for that symbol type
        // to ensure a stable sort. For instance, in the case of functions,
        // we sort by name, then number of parameters, then parameter types,
        // and so on.
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

namespace {
template <class T>
constexpr
auto
toDerivedView(std::vector<SymbolID> const& ids, CorpusImpl& c)
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
    if (corpus_.config->sortMembers)
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
