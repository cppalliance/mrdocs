//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_FUNCTION_HPP
#define MRDOCS_API_METADATA_FUNCTION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/ADT/PolymorphicValue.hpp>
#include <memory>
#include <string>
#include <vector>

namespace clang::mrdocs {

/** Return the name of an operator as a string.

    @param kind The kind of operator.
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

    @param kind The kind of operator.
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

/** Return the FunctionClass from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FunctionClass const kind)
{
    v = toString(kind);
}


// KRYSTIAN TODO: attributes (nodiscard, deprecated, and carries_dependency)
// KRYSTIAN TODO: flag to indicate whether this is a function parameter pack
/** Represents a single function parameter */
struct Param
{
    /** The type of this parameter */
    PolymorphicValue<TypeInfo> Type;

    /** The parameter name.

        Unnamed parameters are represented by empty strings.
    */
    std::string Name;

    /** The default argument for this parameter, if any */
    std::string Default;

    Param() = default;

    Param(
        PolymorphicValue<TypeInfo>&& type,
        std::string&& name,
        std::string&& def_arg)
        : Type(std::move(type))
        , Name(std::move(name))
        , Default(std::move(def_arg))
    {}
};

/** Return the Param as a @ref dom::Value object.
 */
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Param const& p,
    DomCorpus const* domCorpus);

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo final
    : InfoCommonBase<InfoKind::Function>
    , SourceInfo
{
    /// Info about the return type of this function.
    PolymorphicValue<TypeInfo> ReturnType;

    /// List of parameters.
    std::vector<Param> Params;

    /// When present, this function is a template or specialization.
    std::optional<TemplateInfo> Template;

    /// The class of function this is
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

    std::vector<std::string> Attributes;

    //--------------------------------------------

    explicit FunctionInfo(SymbolID const& ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

//------------------------------------------------

} // clang::mrdocs

#endif
