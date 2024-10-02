//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_FUNCTION_HPP
#define MRDOCS_API_METADATA_FUNCTION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Field.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Dom.hpp>
#include <memory>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {

/** Return the name of an operator as a string.

    @param include_keyword Whether the name
    should be prefixed with the `operator` keyword.
*/
MRDOCS_DECL
std::string_view
getOperatorName(
    OperatorKind kind,
    bool include_keyword = false) noexcept;

/** Return the short name of an operator as a string.
*/
MRDOCS_DECL
std::string_view
getShortOperatorName(
    OperatorKind kind) noexcept;

/** Return the safe name of an operator as a string.

    @param include_keyword Whether the name
    should be prefixed with `operator_`.
*/
MRDOCS_DECL
std::string_view
getSafeOperatorName(
    OperatorKind kind,
    bool include_keyword = false) noexcept;

/** Function classifications */
enum class FunctionClass
{
    Normal = 0,
    Constructor,
    // conversion function
    Conversion,
    Destructor
};

MRDOCS_DECL dom::String toString(FunctionClass kind) noexcept;

// KRYSTIAN TODO: attributes (nodiscard, deprecated, and carries_dependency)
// KRYSTIAN TODO: flag to indicate whether this is a function parameter pack
/** Represents a single function parameter */
struct Param
{
    /** The type of this parameter */
    std::unique_ptr<TypeInfo> Type;

    /** The parameter name.

        Unnamed parameters are represented by empty strings.
    */
    std::string Name;

    /** The default argument for this parameter, if any */
    std::string Default;

    Param() = default;

    Param(
        std::unique_ptr<TypeInfo>&& type,
        std::string&& name,
        std::string&& def_arg)
        : Type(std::move(type))
        , Name(std::move(name))
        , Default(std::move(def_arg))
    {
    }
};

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo
    : InfoCommonBase<InfoKind::Function>
    , SourceInfo
{
    std::unique_ptr<TypeInfo> ReturnType; // Info about the return type of this function.
    std::vector<Param> Params; // List of parameters.

    // When present, this function is a template or specialization.
    std::unique_ptr<TemplateInfo> Template;

    // the class of function this is
    FunctionClass Class = FunctionClass::Normal;

    NoexceptInfo Noexcept;

    ExplicitInfo Explicit;

    ExprInfo Requires;

    bool IsVariadic = false;
    bool IsVirtual = false;
    bool IsVirtualAsWritten = false;
    bool IsPure = false;
    bool IsDefaulted = false;
    bool IsExplicitlyDefaulted = false;
    bool IsDeleted = false;
    bool IsDeletedAsWritten = false;
    bool IsNoReturn = false;
    bool HasOverrideAttr = false;
    bool HasTrailingReturn = false;
    bool IsConst = false;
    bool IsVolatile = false;
    bool IsFinal = false;
    bool IsNodiscard = false;
    bool IsExplicitObjectMemberFunction = false;

    ConstexprKind Constexpr = ConstexprKind::None;
    OperatorKind OverloadedOperator = OperatorKind::None;
    StorageClassKind StorageClass = StorageClassKind::None;
    ReferenceKind RefQualifier = ReferenceKind::None;

    //--------------------------------------------

    explicit FunctionInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

//------------------------------------------------

} // mrdocs
} // clang

#endif
