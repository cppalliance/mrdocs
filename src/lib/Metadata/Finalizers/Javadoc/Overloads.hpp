//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_JAVADOC_OVERLOADS_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_JAVADOC_OVERLOADS_HPP

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/Finalizers/Javadoc/Function.hpp>
#include <lib/Metadata/SymbolSet.hpp>
#include <utility>

namespace mrdocs {
namespace {
auto
overloadFunctionsRange(OverloadsSymbol const& O, CorpusImpl& corpus_)
{
    // Create a view all Info members of O
    auto functions =
        O.Members |
        std::views::transform([&](SymbolID const& id)
            {
                return corpus_.find(id);
            }) |
        std::views::filter([](Symbol const* infoPtr)
            {
                return infoPtr != nullptr && infoPtr->isFunction();
            }) |
        std::views::transform([](Symbol const* infoPtr) -> FunctionSymbol const&
            {
                return infoPtr->asFunction();
            });
    return functions;
}

template <class Range>
bool
populateOverloadsBriefIfAllSameBrief(OverloadsSymbol& I, Range&& functionsWithBrief)
{
    auto first = *functionsWithBrief.begin();
    doc::Brief const& firstBrief = *first.javadoc->brief;
    if (auto otherFunctions = std::views::drop(functionsWithBrief, 1);
        std::ranges::all_of(otherFunctions, [&](FunctionSymbol const& otherFunction)
        {
            doc::Brief const& otherBrief = *otherFunction.javadoc->brief;
            return otherBrief.children == firstBrief.children;
        }))
    {
        I.javadoc->brief = firstBrief;
        return true;
    }
    return false;
}

bool
populateOverloadsFromClass(OverloadsSymbol& I)
{
    switch (I.Class)
    {
    case FunctionClass::Normal:
        return false;
    case FunctionClass::Constructor:
    {
        I.javadoc->brief = "Constructors";
        return true;
    }
    case FunctionClass::Destructor:
    {
        I.javadoc->brief = "Destructors";
        return true;
    }
    case FunctionClass::Conversion:
    {
        I.javadoc->brief = "Conversion operators";
        return true;
    }
    default:
        MRDOCS_UNREACHABLE();
    }
}

template <class Range>
bool
populateOverloadsFromOperator(OverloadsSymbol& I, Range&& functions)
{
    MRDOCS_CHECK_OR(I.OverloadedOperator != OperatorKind::None, false);

    // Stream insertion operators are implemented as an exception to the operator name
    if (I.OverloadedOperator == OperatorKind::LessLess &&
        std::ranges::all_of(functions, isStreamInsertion))
    {
        I.javadoc->brief = "Stream insertion operators";
        return true;
    }

    // Find the operator name
    auto isBinary = [&](FunctionSymbol const& function) {
        return (function.Params.size() + function.IsRecordMethod) == 2;
    };
    auto isAllBinary = std::ranges::all_of(functions, isBinary);
    int const nParams = isAllBinary ? 2 : 1;
    auto const res = getOperatorReadableName(I.OverloadedOperator, nParams);
    MRDOCS_CHECK_OR(res, false);
    std::string briefStr(*res);
    briefStr += " operators";
    I.javadoc->brief = std::move(briefStr);
    return true;
}

bool
populateOverloadsFromFunctionName(OverloadsSymbol& I)
{
    std::string name = I.Name;
    if (name.empty() &&
        I.OverloadedOperator != OperatorKind::None)
    {
        name = getOperatorName(I.OverloadedOperator, true);
    }
    if (name.empty())
    {
        return false;
    }
    I.javadoc->brief.emplace();
    I.javadoc->brief->children.emplace_back(
        std::in_place_type<doc::Styled>, std::string(name), doc::Style::mono);
    I.javadoc->brief->children.emplace_back(
        std::in_place_type<doc::Text>, std::string(" overloads"));
    return true;
}

template <class Range>
void
populateOverloadsBrief(OverloadsSymbol& I, Range&& functions, CorpusImpl& corpus)
{
    auto functionsWithBrief = std::views::
        filter(functions, [](FunctionSymbol const& function) {
        return function.javadoc && function.javadoc->brief
               && !function.javadoc->brief->empty();
    });
    auto anyMemberBrief = !std::ranges::empty(functionsWithBrief);
    if (!corpus.config->autoFunctionMetadata &&
        !anyMemberBrief)
    {
        // If there are no briefs, and we'll not populate the briefs
        // from function names, we'll also not populate the briefs
        // of the overload set.
        return;
    }
    if (anyMemberBrief)
    {
        MRDOCS_CHECK_OR(!populateOverloadsBriefIfAllSameBrief(I, functionsWithBrief));
    }
    MRDOCS_CHECK_OR(!populateOverloadsFromClass(I));
    MRDOCS_CHECK_OR(!populateOverloadsFromOperator(I, functions));
    if (anyMemberBrief)
    {
        // We recur to the function name when the briefs are in conflict
        // If there are no briefs, we don't consider it a conflict
        // We just leave the overload set also without a brief
        MRDOCS_CHECK_OR(!populateOverloadsFromFunctionName(I));
    }
}

// Populate with all the unique "returns" from the functions
template <class Range>
void
populateOverloadsReturns(OverloadsSymbol& I, Range&& functions) {
    auto functionReturns = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->returns.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->returns;
            }) |
        std::views::join;
    for (doc::Returns const& functionReturn: functionReturns)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->returns,
            [&functionReturn](doc::Returns const& overloadReturns)
            {
                return overloadReturns == functionReturn;
            });
        if (sameIt == I.javadoc->returns.end())
        {
            I.javadoc->returns.push_back(functionReturn);
        }
    }
}

template <class Range>
void
populateOverloadsParams(OverloadsSymbol& I, Range& functions) {
    auto functionParams = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->params.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->params;
            }) |
        std::views::join;
    for (doc::Param const& functionParam: functionParams)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->params,
            [&functionParam](doc::Param const& overloadParam)
            {
                return overloadParam.name == functionParam.name;
            });
        if (sameIt == I.javadoc->params.end())
        {
            I.javadoc->params.push_back(functionParam);
        }
    }
}

template <class Range>
void
populateOverloadsTParams(OverloadsSymbol& I, Range& functions) {
    auto functionTParams = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->tparams.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->tparams;
            }) |
        std::views::join;
    for (doc::TParam const& functionTParam: functionTParams)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->tparams,
            [&functionTParam](doc::TParam const& overloadTParam)
            {
                return overloadTParam.name == functionTParam.name;
            });
        if (sameIt == I.javadoc->tparams.end())
        {
            I.javadoc->tparams.push_back(functionTParam);
        }
    }
}

template <class Range>
void
populateOverloadsExceptions(OverloadsSymbol& I, Range& functions) {
    auto functionExceptions = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->exceptions.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->exceptions;
            }) |
        std::views::join;
    for (doc::Throws const& functionException: functionExceptions)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->exceptions,
            [&functionException](doc::Throws const& overloadException)
            {
                return overloadException.exception.string == functionException.exception.string;
            });
        if (sameIt == I.javadoc->exceptions.end())
        {
            I.javadoc->exceptions.push_back(functionException);
        }
    }
}

template <class Range>
void
populateOverloadsSees(OverloadsSymbol& I, Range& functions) {
    auto functionSees = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->sees.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->sees;
            }) |
        std::views::join;
    for (doc::See const& functionSee: functionSees)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->sees,
            [&functionSee](doc::See const& overloadSee)
            {
                return overloadSee.children == functionSee.children;
            });
        if (sameIt == I.javadoc->sees.end())
        {
            I.javadoc->sees.push_back(functionSee);
        }
    }
}

template <class Range>
void
populateOverloadsPreconditions(OverloadsSymbol& I, Range& functions) {
    auto functionsPres = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->preconditions.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->preconditions;
            }) |
        std::views::join;
    for (doc::Precondition const& functionPre: functionsPres)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->preconditions,
            [&functionPre](doc::Precondition const& overloadPre)
            {
                return overloadPre.children == functionPre.children;
            });
        if (sameIt == I.javadoc->preconditions.end())
        {
            I.javadoc->preconditions.push_back(functionPre);
        }
    }
}

template <class Range>
void
populateOverloadsPostconditions(OverloadsSymbol& I, Range& functions) {
    auto functionsPosts = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.javadoc && !function.javadoc->postconditions.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.javadoc->postconditions;
            }) |
        std::views::join;
    for (doc::Postcondition const& functionPost: functionsPosts)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->postconditions,
            [&functionPost](doc::Postcondition const& overloadPost)
            {
                return overloadPost.children == functionPost.children;
            });
        if (sameIt == I.javadoc->postconditions.end())
        {
            I.javadoc->postconditions.push_back(functionPost);
        }
    }
}

}
} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_JAVADOC_OVERLOADS_HPP
