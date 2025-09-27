//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_JAVADOC_FUNCTION_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_JAVADOC_FUNCTION_HPP

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/InfoSet.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <format>
#include <utility>

namespace clang::mrdocs {
namespace {
bool
isSpecialFunction(FunctionInfo const& I)
{
    return
        I.Class != FunctionClass::Normal ||
        I.OverloadedOperator != OperatorKind::None;
}

bool
isDefaultConstructor(FunctionInfo const& I)
{
    return I.Class == FunctionClass::Constructor && I.Params.empty();
}

template <bool move, bool assignment>
bool
isCopyOrMoveConstructorOrAssignment(FunctionInfo const& I)
{
    if constexpr (!assignment)
    {
        MRDOCS_CHECK_OR(I.Class == FunctionClass::Constructor, false);
    }
    else
    {
        MRDOCS_CHECK_OR(I.OverloadedOperator == OperatorKind::Equal, false);
    }
    MRDOCS_CHECK_OR(I.Params.size() == 1, false);
    auto const& param = I.Params[0];
    Polymorphic<TypeInfo> const& paramType = param.Type;
    MRDOCS_ASSERT(!paramType.valueless_after_move());
    if constexpr (!move)
    {
        MRDOCS_CHECK_OR(paramType->isLValueReference(), false);
    }
    else
    {
        MRDOCS_CHECK_OR(paramType->isRValueReference(), false);
    }
    using RefType = std::conditional_t<move, RValueReferenceTypeInfo, LValueReferenceTypeInfo>;
    auto const& paramRefType = static_cast<RefType const &>(*paramType);
    auto const& paramRefPointeeOpt = paramRefType.PointeeType;
    MRDOCS_CHECK_OR(paramRefPointeeOpt, false);
    TypeInfo const& paramRefPointee = *paramRefPointeeOpt;
    auto const *paramRefPointeeNamed = paramRefPointee.asNamedPtr();
    MRDOCS_CHECK_OR(paramRefPointeeNamed, false);
    MRDOCS_CHECK_OR(paramRefPointeeNamed->Name, false);
    auto const& paramName = paramRefPointeeNamed->Name;
    MRDOCS_CHECK_OR(paramName, false);
    auto const& paramRefPointeeNamedName = paramName->Name;
    MRDOCS_CHECK_OR(!paramRefPointeeNamedName.empty(), false);
    SymbolID const& id = paramName->id;
    MRDOCS_CHECK_OR(id, false);
    return id == I.Parent;
}

bool
isCopyConstructor(FunctionInfo const& I)
{
    return isCopyOrMoveConstructorOrAssignment<false, false>(I);
}

bool
isMoveConstructor(FunctionInfo const& I)
{
    return isCopyOrMoveConstructorOrAssignment<true, false>(I);
}

bool
isCopyAssignment(FunctionInfo const& I)
{
    return isCopyOrMoveConstructorOrAssignment<false, true>(I);
}

bool
isMoveAssignment(FunctionInfo const& I)
{
    return isCopyOrMoveConstructorOrAssignment<true, true>(I);
}

Optional<std::string_view>
innermostTypenameString(Polymorphic<TypeInfo> const& T)
{
    auto& R = innermostType(T);
    MRDOCS_CHECK_OR(R->isNamed(), {});
    MRDOCS_CHECK_OR(dynamic_cast<NamedTypeInfo const &>(*R).Name, {});
    MRDOCS_CHECK_OR(!dynamic_cast<NamedTypeInfo const &>(*R).Name->Name.empty(),
                    {});
    auto& RStr = dynamic_cast<const NamedTypeInfo &>(*R).Name->Name;
    return RStr;
}

//Optional<std::string_view>
//innermostTypenameString(Optional<Polymorphic<TypeInfo>> const& T)
//{
//    MRDOCS_CHECK_OR(T, {});
//    return innermostTypenameString(*T);
//}

bool
populateFunctionBriefFromClass(FunctionInfo& I, CorpusImpl const& corpus)
{
    switch (I.Class)
    {
        case FunctionClass::Normal:
            return false;
        case FunctionClass::Constructor:
        {
            if (isDefaultConstructor(I))
            {
                I.javadoc->brief = "Default constructor";
                return true;
            }
            if (isCopyConstructor(I))
            {
                I.javadoc->brief = "Copy constructor";
                return true;
            }
            if (isMoveConstructor(I))
            {
                I.javadoc->brief = "Move constructor";
                return true;
            }
            if (I.Params.size() == 1)
            {
                auto const& param = I.Params[0];
                if (auto const res = innermostTypenameString(param.Type))
                {
                    if (!I.javadoc->brief)
                    {
                        I.javadoc->brief.emplace();
                    }
                    I.javadoc->brief->children.clear();
                    I.javadoc->brief->children.emplace_back(
                        Polymorphic<doc::Text>(std::in_place_type<doc::Text>,
                                               std::string("Construct from ")));
                    I.javadoc->brief->children.emplace_back(
                        Polymorphic<doc::Text>(std::in_place_type<doc::Styled>,
                                               std::string(*res),
                                               doc::Style::mono));
                    return true;
                }
            }
            I.javadoc->brief = "Constructor";
            return true;
        }
        case FunctionClass::Destructor:
        {
            I.javadoc->brief = "Destructor";
            return true;
        }
        case FunctionClass::Conversion:
        {
            if (auto const R = innermostTypenameString(I.ReturnType))
            {
                auto const RStr = *R;
                I.javadoc->brief.emplace();
                I.javadoc->brief->children.emplace_back(
                    Polymorphic<doc::Text>(std::in_place_type<doc::Text>,
                                           std::string("Conversion to ")));
                I.javadoc->brief->children.emplace_back(Polymorphic<doc::Text>(
                    std::in_place_type<doc::Styled>, std::string(RStr),
                    doc::Style::mono));
            }
            else
            {
                I.javadoc->brief = "Conversion operator";
            }
            return true;
        }
        default:
            MRDOCS_UNREACHABLE();
    }
}

bool
isStreamInsertion(FunctionInfo const& function)
{
    MRDOCS_CHECK_OR(!function.IsRecordMethod, false);
    MRDOCS_CHECK_OR(function.Params.size() == 2, false);
    MRDOCS_CHECK_OR(function.OverloadedOperator == OperatorKind::LessLess, false);
    // Check first param is a mutable left reference
    auto& firstParam = function.Params[0];
    MRDOCS_CHECK_OR(firstParam, false);
    Polymorphic<TypeInfo> const& firstQualType = firstParam.Type;
    MRDOCS_ASSERT(!firstQualType.valueless_after_move());
    MRDOCS_CHECK_OR(firstQualType->isLValueReference(), false);
    auto& firstNamedTypeOpt =
        dynamic_cast<LValueReferenceTypeInfo const &>(*firstQualType)
            .PointeeType;
    MRDOCS_CHECK_OR(firstNamedTypeOpt, false);
    auto& firstNamedType = *firstNamedTypeOpt;
    MRDOCS_CHECK_OR(firstNamedType.isNamed(), false);
    // Check return type
    return firstQualType == function.ReturnType;
}

bool
populateFunctionBriefFromOperator(FunctionInfo& I)
{
    MRDOCS_CHECK_OR(I.OverloadedOperator != OperatorKind::None, false);

    // Stream insertion operators are implemented as an exception to the operator name
    if (isStreamInsertion(I))
    {
        I.javadoc->brief = "Stream insertion operator";
        return true;
    }

    if (isCopyAssignment(I))
    {
        I.javadoc->brief = "Copy assignment operator";
        return true;
    }

    if (isMoveAssignment(I))
    {
        I.javadoc->brief = "Move assignment operator";
        return true;
    }

    // Find the operator name
    auto const res =
        getOperatorReadableName(
            I.OverloadedOperator,
            I.Params.size() + I.IsRecordMethod);
    MRDOCS_CHECK_OR(res, false);
    std::string briefStr(*res);
    briefStr += " operator";
    I.javadoc->brief = std::move(briefStr);
    return true;
}

void
populateFunctionBrief(FunctionInfo& I, CorpusImpl const& corpus)
{
    MRDOCS_CHECK_OR(!I.javadoc->brief);
    MRDOCS_CHECK_OR(!populateFunctionBriefFromClass(I, corpus));
    MRDOCS_CHECK_OR(!populateFunctionBriefFromOperator(I));
}




Info const*
getInfo(
    Polymorphic<TypeInfo> const& R,
    CorpusImpl const& corpus)
{
    SymbolID const id = R->namedSymbol();
    MRDOCS_CHECK_OR(id, nullptr);
    Info const* Rinfo = corpus.find(id);
    return Rinfo;
}

doc::Brief const*
getInfoBrief(
    Polymorphic<TypeInfo> const& R,
    CorpusImpl const& corpus)
{
    Info const* Rinfo = getInfo(R, corpus);
    MRDOCS_CHECK_OR(Rinfo, nullptr);
    MRDOCS_CHECK_OR(Rinfo->javadoc, nullptr);
    MRDOCS_CHECK_OR(Rinfo->javadoc->brief, nullptr);
    return &*Rinfo->javadoc->brief;
}

bool
populateFunctionReturnsFromFunctionBrief(
    FunctionInfo& I)
{
    MRDOCS_CHECK_OR(I.javadoc->brief, false);
    MRDOCS_CHECK_OR(I.javadoc->brief->children.size() == 1, false);
    auto& brief = I.javadoc->brief->children[0];
    MRDOCS_CHECK_OR(brief->Kind == doc::NodeKind::text, false);
    std::string_view briefText = brief->string;
    static constexpr std::string_view briefPrefixes[] = {
        "Returns ",
        "Return ",
        "Get ",
        "Gets ",
        "Determine ",
        "Determines ",
    };
    for (std::string_view prefix: briefPrefixes)
    {
        if (briefText.starts_with(prefix))
        {
            briefText.remove_prefix(prefix.size());
            I.javadoc->returns.emplace_back(std::string(briefText));
            return true;
        }
    }
    return false;
}

bool
populateFunctionReturnsForSpecial(
    FunctionInfo& I,
    Polymorphic<TypeInfo> const& innerR,
    CorpusImpl const& corpus)
{
    if (I.Class == FunctionClass::Conversion)
    {
        if (auto* brief = getInfoBrief(innerR, corpus))
        {
            I.javadoc->returns.emplace_back(*brief);
            return true;
        }
        auto const exp = innermostTypenameString(innerR);
        MRDOCS_CHECK_OR(exp, false);
        doc::Returns r;
        r.children.emplace_back(
            Polymorphic<doc::Text>(std::in_place_type<doc::Text>,
                                   std::string("The object converted to ")));
        r.children.emplace_back(
            Polymorphic<doc::Text>(std::in_place_type<doc::Styled>,
                                   std::string(*exp), doc::Style::mono));
        I.javadoc->returns.emplace_back(std::move(r));
        return true;
    }
    MRDOCS_CHECK_OR(I.OverloadedOperator != OperatorKind::None, false);

    // Stream operator
    if (isStreamInsertion(I))
    {
        I.javadoc->returns.emplace_back("Reference to the current output stream");
        return true;
    }

    // Special functions that should return a reference to self
    MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
    if (I.ReturnType->isLValueReference())
    {
        Info const* RInfo = getInfo(innerR, corpus);
        MRDOCS_CHECK_OR(RInfo, false);
        MRDOCS_CHECK_OR(RInfo->id == I.Parent, false);
        I.javadoc->returns.emplace_back("Reference to the current object");
        return true;
    }
    else if (I.ReturnType->isPointer())
    {
        Info const* RInfo = getInfo(innerR, corpus);
        MRDOCS_CHECK_OR(RInfo, false);
        MRDOCS_CHECK_OR(RInfo->id == I.Parent, false);
        I.javadoc->returns.emplace_back("Pointer to the current object");
        return true;
    }

    // Special functions that can return bool
    if (is_one_of(
            I.OverloadedOperator,
            { OperatorKind::EqualEqual,
              OperatorKind::ExclaimEqual,
              OperatorKind::Less,
              OperatorKind::LessEqual,
              OperatorKind::Greater,
              OperatorKind::GreaterEqual,
              OperatorKind::Exclaim }))
    {
        MRDOCS_CHECK_OR(I.ReturnType, false);
        MRDOCS_CHECK_OR(I.ReturnType->isNamed(), false);
        MRDOCS_CHECK_OR(
            dynamic_cast<NamedTypeInfo &>(*I.ReturnType).FundamentalType ==
                FundamentalTypeKind::Bool,
            false);
        doc::Returns r;
        r.children.emplace_back(
            Polymorphic<doc::Text>(std::in_place_type<doc::Styled>,
                                   std::string("true"), doc::Style::mono));
        std::string_view midText;
        switch (I.OverloadedOperator)
        {
            case OperatorKind::EqualEqual:
                midText = " if the objects are equal, ";
                break;
            case OperatorKind::ExclaimEqual:
                midText = " if the objects are not equal, ";
                break;
            case OperatorKind::Less:
                midText = " if the left object is less than the right object, ";
                break;
            case OperatorKind::LessEqual:
                midText = " if the left object is less than or equal to the right object, ";
                break;
            case OperatorKind::Greater:
                midText = " if the left object is greater than the right object, ";
                break;
            case OperatorKind::GreaterEqual:
                midText = " if the left object is greater than or equal to the right object, ";
                break;
            case OperatorKind::Exclaim:
                midText = " if the object is falsy, ";
                break;
            default:
                MRDOCS_UNREACHABLE();
        }
        r.children.emplace_back(Polymorphic<doc::Text>(
            std::in_place_type<doc::Text>, std::string(midText)));
        r.children.emplace_back(
            Polymorphic<doc::Text>(std::in_place_type<doc::Styled>,
                                   std::string("false"), doc::Style::mono));
        r.children.emplace_back(Polymorphic<doc::Text>(
            std::in_place_type<doc::Text>, std::string(" otherwise")));
        I.javadoc->returns.emplace_back(std::move(r));
        return false;
    }

    // Spaceship operator
    if (I.OverloadedOperator == OperatorKind::Spaceship)
    {
        I.javadoc->returns.emplace_back("The relative order of the objects");
        return true;
    }

    // Special function that return the same type as the parent
    MRDOCS_ASSERT(!innerR.valueless_after_move());
    if (I.IsRecordMethod &&
        innerR->isNamed() &&
        dynamic_cast<NamedTypeInfo const &>(*innerR).Name->id &&
        dynamic_cast<NamedTypeInfo const &>(*innerR).Name->id == I.Parent)
    {
        MRDOCS_CHECK_OR(I.ReturnType, false);
        MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
        if (I.ReturnType->isLValueReference())
        {
            I.javadoc->returns.emplace_back("Reference to the current object");
        }
        else if (I.ReturnType->isRValueReference())
        {
            I.javadoc->returns.emplace_back(
                "Rvalue reference to the current object");
        }
        else if (I.ReturnType->isPointer())
        {
            I.javadoc->returns.emplace_back("Pointer to the current object");
        }
        else
        {
            I.javadoc->returns.emplace_back("Another instance of the object");
        }
        return true;
    }

    return false;
}

bool
populateFunctionReturnsFromReturnTypeBrief(
    FunctionInfo& I,
    Polymorphic<TypeInfo> const& innerR,
    CorpusImpl const& corpus)
{
    if (auto* brief = getInfoBrief(innerR, corpus))
    {
        I.javadoc->returns.emplace_back(*brief);
        return true;
    }
    return false;
}

void
populateFunctionReturns(FunctionInfo& I, CorpusImpl const& corpus)
{
    MRDOCS_CHECK_OR(I.javadoc->returns.empty());

    // Populate the return doc from brief of the function
    // when the brief is "Returns ..."
    MRDOCS_CHECK_OR(!populateFunctionReturnsFromFunctionBrief(I));

    // Check if we have a named return type
    MRDOCS_CHECK_OR(I.ReturnType);
    MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
    auto& inner = innermostType(I.ReturnType);
    MRDOCS_CHECK_OR(inner);
    if (inner->isNamed())
    {
        auto& Ninner = dynamic_cast<NamedTypeInfo const &>(*inner);
        MRDOCS_CHECK_OR(Ninner.Name);
        MRDOCS_CHECK_OR(!Ninner.Name->Name.empty());
        MRDOCS_CHECK_OR(Ninner.FundamentalType != FundamentalTypeKind::Void);
    }

    // If we have a named return type, we can populate the returns
    // Populate the return doc for special functions
    MRDOCS_CHECK_OR(!populateFunctionReturnsForSpecial(I, inner, corpus));

    // Populate the return doc from the return type brief
    MRDOCS_CHECK_OR(!populateFunctionReturnsFromReturnTypeBrief(I, inner, corpus));
}

/* Get a list of all parameter names in javadoc

    The javadoc parameter names can contain a single parameter or
    a list of parameters separated by commas. This function
    returns a list of all parameter names in the javadoc.
 */
SmallVector<std::string_view, 32>
getJavadocParamNames(Javadoc const& javadoc)
{
    SmallVector<std::string_view, 32> result;
    for (auto const& javadocParam: javadoc.params)
    {
        auto const& paramNamesStr = javadocParam.name;
        for (auto paramNames = std::views::split(paramNamesStr, ',');
             auto const& paramName: paramNames)
        {
            result.push_back(trim(std::string_view(paramName.begin(), paramName.end())));
        }
    }
    return result;
}

bool
setCntrOrAssignParamName(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR(index == 0, false);
    MRDOCS_CHECK_OR(I.Params.size() == 1, false);
    MRDOCS_CHECK_OR(I.IsRecordMethod, false);
    MRDOCS_CHECK_OR(
        I.Class == FunctionClass::Constructor ||
        I.OverloadedOperator == OperatorKind::Equal,
        false);
    auto paramNames =
        std::views::filter(I.Params, [](Param const& p) -> bool {
            return p.Name.has_value();
        }) |
        std::views::transform([](Param const& p) -> std::string_view {
            return *p.Name;
        });
    MRDOCS_CHECK_OR(param.Type, false);
    MRDOCS_ASSERT(!param.Type.valueless_after_move());
    Polymorphic<TypeInfo>& innerP = innermostType(param.Type);
    std::string_view paramName = "value";
    if (innerP->namedSymbol() == I.Parent)
    {
        paramName = "other";
    }
    MRDOCS_CHECK_OR(!contains(paramNames, paramName), false);
    param.Name = paramName;
    return true;
}

bool
setStreamOperatorParamName(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR(index < 2, false);
    MRDOCS_CHECK_OR(isStreamInsertion(I), false);
    auto paramNames =
        std::views::filter(I.Params, [](Param const& p) -> bool {
        return p.Name.has_value();
    }) |
        std::views::transform([](Param const& p) -> std::string_view {
        return *p.Name;
    });
    std::string_view paramName =
        index == 0 ? "os" : "value";
    MRDOCS_CHECK_OR(!contains(paramNames, paramName), false);
    param.Name = paramName;
    return true;
}

bool
setBinaryOpParamName(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR((I.IsRecordMethod && index == 0) || index < 2, false);
    MRDOCS_CHECK_OR(isBinaryOperator(I.OverloadedOperator), false);
    std::size_t const sizeFree = I.IsRecordMethod ? I.Params.size() + 1 : I.Params.size();
    MRDOCS_CHECK_OR(sizeFree == 2, false);

    auto paramNames =
        std::views::filter(I.Params, [](Param const& p) -> bool {
        return p.Name.has_value();
    }) |
        std::views::transform([](Param const& p) -> std::string_view {
        return *p.Name;
    });
    std::size_t const indexFree = I.IsRecordMethod ? index + 1 : index;
    std::string_view paramName = indexFree == 0 ? "lhs" : "rhs";
    MRDOCS_CHECK_OR(!contains(paramNames, paramName), false);
    param.Name = paramName;
    return true;
}

bool
setUnaryOpParamName(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR(!I.IsRecordMethod, false);
    MRDOCS_CHECK_OR(index == 0, false);
    MRDOCS_CHECK_OR(isUnaryOperator(I.OverloadedOperator), false);
    MRDOCS_CHECK_OR(I.Params.size() == 1, false);

    auto paramNames =
        std::views::filter(I.Params, [](Param const& p) -> bool {
        return p.Name.has_value();
    }) |
        std::views::transform([](Param const& p) -> std::string_view {
        return *p.Name;
    });
    std::string_view paramName = "value";
    MRDOCS_CHECK_OR(!contains(paramNames, paramName), false);
    param.Name = paramName;
    return true;
}

bool
setSpecialFunctionParamName(
    FunctionInfo& I,
    Param& param,
    std::size_t index)
{
    MRDOCS_CHECK_OR(!setCntrOrAssignParamName(I, param, index), true);
    MRDOCS_CHECK_OR(!setStreamOperatorParamName(I, param, index), true);
    MRDOCS_CHECK_OR(!setBinaryOpParamName(I, param, index), true);
    MRDOCS_CHECK_OR(!setUnaryOpParamName(I, param, index), true);
    return false;
}

bool
setCntrOrAssignParamDoc(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR(index == 0, false);
    MRDOCS_CHECK_OR(I.IsRecordMethod, false);
    MRDOCS_CHECK_OR(
        I.Class == FunctionClass::Constructor ||
        I.OverloadedOperator == OperatorKind::Equal,
        false);

    // Set the parameter description if we can
    MRDOCS_CHECK_OR(param, false);
    MRDOCS_CHECK_OR(param.Type, false);
    MRDOCS_ASSERT(!param.Type.valueless_after_move());
    auto& innerParam = innermostType(param.Type);
    MRDOCS_CHECK_OR(innerParam, false);
    MRDOCS_CHECK_OR(innerParam->isNamed(), false);
    std::string_view const paramNoun
        = dynamic_cast<NamedTypeInfo &>(*innerParam).FundamentalType ? "value"
                                                                    : "object";
    std::string_view const functionVerb = [&]() {
        bool const isAssign = I.OverloadedOperator == OperatorKind::Equal;
        if (dynamic_cast<NamedTypeInfo &>(*innerParam).FundamentalType)
        {
            return !isAssign ? "construct" : "assign";
        }
        MRDOCS_ASSERT(!param.Type.valueless_after_move());
        if (param.Type->isLValueReference())
        {
            return !isAssign ? "copy construct" : "copy assign";
        }
        if (param.Type->isRValueReference())
        {
            return !isAssign ? "move construct" : "move assign";
        }
        return !isAssign ? "construct" : "assign";
    }();
    I.javadoc->params.emplace_back(
        *param.Name, std::format("The {} to {} from", paramNoun, functionVerb));
    return true;
}

bool
setBinaryOpParamDoc(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    std::size_t const indexFree = I.IsRecordMethod ? index + 1 : index;
    std::size_t const sizeFree = I.IsRecordMethod ? I.Params.size() + 1 : I.Params.size();
    MRDOCS_CHECK_OR(indexFree < 2, false);
    MRDOCS_CHECK_OR(isBinaryOperator(I.OverloadedOperator), false);
    MRDOCS_CHECK_OR(sizeFree == 2, false);

    // Set the parameter description if we can
    std::string_view const paramAdj = indexFree == 0 ? "left" : "right";
    I.javadoc->params.emplace_back(*param.Name,
                                   std::format("The {} operand", paramAdj));
    return true;
}

bool
setUnaryOpParamDoc(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR(!I.IsRecordMethod, false);
    MRDOCS_CHECK_OR(index == 0, false);
    MRDOCS_CHECK_OR(isUnaryOperator(I.OverloadedOperator), false);
    MRDOCS_CHECK_OR(I.Params.size() == 1, false);

    // Set the parameter description if we can
    I.javadoc->params.emplace_back(*param.Name, "The operand");
    return true;
}

bool
setStreamOperatorParamDoc(
    FunctionInfo& I,
    Param& param,
    std::size_t const index)
{
    MRDOCS_CHECK_OR(index < 2, false);
    MRDOCS_CHECK_OR(isStreamInsertion(I), false);
    if (index == 0)
    {
        I.javadoc->params.emplace_back(*param.Name, "An output stream");
    }
    else
    {
        I.javadoc->params.emplace_back(*param.Name, "The object to output");
    }
    return true;
}

void
setFunctionParamDoc(
    FunctionInfo& I,
    Param& param,
    std::size_t const index,
    CorpusImpl const& corpus)
{
    if (setCntrOrAssignParamDoc(I, param, index))
    {
        return;
    }
    if (setStreamOperatorParamDoc(I, param, index))
    {
        return;
    }
    if (setBinaryOpParamDoc(I, param, index))
    {
        return;
    }

    if (setUnaryOpParamDoc(I, param, index))
    {
        return;
    }

    // param is a named parameter: use the brief of the type
    // as a description for the parameter
    MRDOCS_ASSERT(!param.Type.valueless_after_move());
    auto const& innerParam = innermostType(param.Type);
    doc::Brief const* paramBrief = getInfoBrief(innerParam, corpus);
    MRDOCS_CHECK_OR(paramBrief);
    I.javadoc->params.emplace_back(*param.Name, *paramBrief);
}

void
populateFunctionParam(
    FunctionInfo& I,
    Param& param,
    std::size_t const index,
    SmallVector<std::string_view, 32>& documentedParams,
    CorpusImpl const& corpus)
{
    if (!param.Name)
    {
        setSpecialFunctionParamName(
            I,
            param,
            index);
    }
    MRDOCS_CHECK_OR(param.Name);
    MRDOCS_CHECK_OR(!contains(documentedParams, *param.Name));
    setFunctionParamDoc(I, param, index, corpus);
}

void
populateFunctionParams(FunctionInfo& I, CorpusImpl const& corpus)
{
    auto documentedParams = getJavadocParamNames(*I.javadoc);
    for (std::size_t i = 0; i < I.Params.size(); ++i)
    {
        populateFunctionParam(I, I.Params[i], i, documentedParams, corpus);
    }
}

}
} // clang::mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_JAVADOC_FUNCTION_HPP
