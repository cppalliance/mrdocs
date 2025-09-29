//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENT_OVERLOADS_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENT_OVERLOADS_HPP

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/Finalizers/DocComment/Function.hpp>
#include <lib/Metadata/SymbolSet.hpp>
#include <utility>

namespace mrdocs {
namespace {

// Create a view of all functions in an overload set
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
    doc::BriefBlock const& firstBrief = *first.doc->brief;
    if (auto otherFunctions = std::views::drop(functionsWithBrief, 1);
        std::ranges::all_of(otherFunctions, [&](FunctionSymbol const& otherFunction)
        {
            doc::BriefBlock const& otherBrief = *otherFunction.doc->brief;
            return otherBrief.children == firstBrief.children;
        }))
    {
        I.doc->brief = firstBrief;
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
        I.doc->brief = "Constructors";
        return true;
    }
    case FunctionClass::Destructor:
    {
        I.doc->brief = "Destructors";
        return true;
    }
    case FunctionClass::Conversion:
    {
        I.doc->brief = "Conversion operators";
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
        I.doc->brief = "Stream insertion operators";
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
    I.doc->brief = std::move(briefStr);
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
    I.doc->brief.emplace();
    I.doc->brief->emplace_back<doc::CodeInline>(name);
    I.doc->brief->children.emplace_back(
        std::in_place_type<doc::TextInline>, std::string(" overloads"));
    return true;
}

// Populate the brief of an overload set according to the following rules:
// 1. If all functions have the same brief, use that brief
// 2. Otherwise, if the overload set is for a special function (constructor, destructor
//    or conversion operator), use a generic brief according to the function class
// 3. Otherwise, if the overload set is for an operator, use a generic brief
//    according to the operator name
// 4. Otherwise, if any function has a brief, use the function name as the brief
// 5. Otherwise, do not populate the brief
//
template <class Range>
void
populateOverloadsBrief(OverloadsSymbol& I, Range&& functions, CorpusImpl& corpus)
{
    auto functionsWithBrief = std::views::
        filter(functions, [](FunctionSymbol const& function) {
        return function.doc && function.doc->brief
               && !function.doc->brief->empty();
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
                return function.doc && !function.doc->returns.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->returns;
            }) |
        std::views::join;
    for (doc::ReturnsBlock const& functionReturn: functionReturns)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->returns,
            [&functionReturn](doc::ReturnsBlock const& overloadReturns)
            {
                return overloadReturns == functionReturn;
            });
        if (sameIt == I.doc->returns.end())
        {
            I.doc->returns.push_back(functionReturn);
        }
    }
}

template <class Range>
void
populateOverloadsParams(OverloadsSymbol& I, Range& functions) {
    auto functionParams = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.doc && !function.doc->params.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->params;
            }) |
        std::views::join;
    for (doc::ParamBlock const& functionParam: functionParams)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->params,
            [&functionParam](doc::ParamBlock const& overloadParam)
            {
                return overloadParam.name == functionParam.name;
            });
        if (sameIt == I.doc->params.end())
        {
            I.doc->params.push_back(functionParam);
        }
    }
}

template <class Range>
void
populateOverloadsTParams(OverloadsSymbol& I, Range& functions) {
    auto functionTParams = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.doc && !function.doc->tparams.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->tparams;
            }) |
        std::views::join;
    for (doc::TParamBlock const& functionTParam: functionTParams)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->tparams,
            [&functionTParam](doc::TParamBlock const& overloadTParam)
            {
                return overloadTParam.name == functionTParam.name;
            });
        if (sameIt == I.doc->tparams.end())
        {
            I.doc->tparams.push_back(functionTParam);
        }
    }
}

template <class Range>
void
populateOverloadsExceptions(OverloadsSymbol& I, Range& functions) {
    auto functionExceptions = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.doc && !function.doc->exceptions.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->exceptions;
            }) |
        std::views::join;
    for (doc::ThrowsBlock const& functionException: functionExceptions)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->exceptions,
            [&functionException](doc::ThrowsBlock const& overloadException)
            {
                return overloadException.exception.literal == functionException.exception.literal;
            });
        if (sameIt == I.doc->exceptions.end())
        {
            I.doc->exceptions.push_back(functionException);
        }
    }
}

template <class Range>
void
populateOverloadsSees(OverloadsSymbol& I, Range& functions) {
    auto functionSees = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.doc && !function.doc->sees.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->sees;
            }) |
        std::views::join;
    for (doc::SeeBlock const& functionSee: functionSees)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->sees,
            [&functionSee](doc::SeeBlock const& overloadSee)
            {
                return overloadSee.children == functionSee.children;
            });
        if (sameIt == I.doc->sees.end())
        {
            I.doc->sees.push_back(functionSee);
        }
    }
}

template <class Range>
void
populateOverloadsPreconditions(OverloadsSymbol& I, Range& functions) {
    auto functionsPres = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.doc && !function.doc->preconditions.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->preconditions;
            }) |
        std::views::join;
    for (doc::PreconditionBlock const& functionPre: functionsPres)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->preconditions,
            [&functionPre](doc::PreconditionBlock const& overloadPre)
            {
                return overloadPre.children == functionPre.children;
            });
        if (sameIt == I.doc->preconditions.end())
        {
            I.doc->preconditions.push_back(functionPre);
        }
    }
}

template <class Range>
void
populateOverloadsPostconditions(OverloadsSymbol& I, Range& functions) {
    auto functionsPosts = functions |
        std::views::filter([](FunctionSymbol const& function)
            {
                return function.doc && !function.doc->postconditions.empty();
            }) |
        std::views::transform([](FunctionSymbol const& function)
            {
                return function.doc->postconditions;
            }) |
        std::views::join;
    for (doc::PostconditionBlock const& functionPost: functionsPosts)
    {
        auto sameIt = std::ranges::find_if(
            I.doc->postconditions,
            [&functionPost](doc::PostconditionBlock const& overloadPost)
            {
                return overloadPost.children == functionPost.children;
            });
        if (sameIt == I.doc->postconditions.end())
        {
            I.doc->postconditions.push_back(functionPost);
        }
    }
}

}
} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENT_OVERLOADS_HPP
