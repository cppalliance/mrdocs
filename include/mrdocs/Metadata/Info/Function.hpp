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
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
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

/** Return the short name of an operator as a string.
*/
MRDOCS_DECL
OperatorKind
getOperatorKind(std::string_view name) noexcept;

/** Return the short name of an operator as a string.
*/
MRDOCS_DECL
OperatorKind
getOperatorKindFromSuffix(std::string_view suffix) noexcept;

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
struct Param final
{
    /** The type of this parameter
     */
    Polymorphic<TypeInfo> Type;

    /** The parameter name.
     */
    Optional<std::string> Name;

    /** The default argument for this parameter, if any
      */
    Optional<std::string> Default;

    Param() = default;

    Param(
        Polymorphic<TypeInfo>&& type,
        std::string&& name,
        std::string&& def_arg)
        : Type(std::move(type))
        , Name(std::move(name))
        , Default(std::move(def_arg))
    {}

    auto
    operator<=>(Param const&) const = default;
};

MRDOCS_DECL
void
merge(Param& I, Param&& Other);

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
{
    /// Info about the return type of this function.
    Polymorphic<TypeInfo> ReturnType;

    /// List of parameters.
    std::vector<Param> Params;

    /// When present, this function is a template or specialization.
    std::optional<TemplateInfo> Template;

    /// The class of function this is
    FunctionClass Class = FunctionClass::Normal;

    NoexceptInfo Noexcept;
    ExprInfo Requires;
    bool IsVariadic = false;
    bool IsDefaulted = false;
    bool IsExplicitlyDefaulted = false;
    bool IsDeleted = false;
    bool IsDeletedAsWritten = false;
    bool HasTrailingReturn = false;
    bool IsExplicitObjectMemberFunction = false;
    ConstexprKind Constexpr = ConstexprKind::None;
    OperatorKind OverloadedOperator = OperatorKind::None;
    StorageClassKind StorageClass = StorageClassKind::None;
    std::vector<std::string> Attributes;

    // CXXMethodDecl
    bool IsRecordMethod = false;
    bool IsVirtual = false;
    bool IsVirtualAsWritten = false;
    bool IsPure = false;
    bool IsConst = false;
    bool IsVolatile = false;
    bool IsFinal = false;
    bool IsOverride = false;
    ReferenceKind RefQualifier = ReferenceKind::None;
    ExplicitInfo Explicit;

    //--------------------------------------------

    explicit FunctionInfo(SymbolID const& ID) noexcept
        : InfoCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(FunctionInfo const& other) const;
};

MRDOCS_DECL
void
merge(FunctionInfo& I, FunctionInfo&& Other);

/** Map a FunctionInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    FunctionInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("isVariadic", I.IsVariadic);
    io.map("isVirtual", I.IsVirtual);
    io.map("isVirtualAsWritten", I.IsVirtualAsWritten);
    io.map("isPure", I.IsPure);
    io.map("isDefaulted", I.IsDefaulted);
    io.map("isExplicitlyDefaulted", I.IsExplicitlyDefaulted);
    io.map("isDeleted", I.IsDeleted);
    io.map("isDeletedAsWritten", I.IsDeletedAsWritten);
    io.map("hasTrailingReturn", I.HasTrailingReturn);
    io.map("isConst", I.IsConst);
    io.map("isVolatile", I.IsVolatile);
    io.map("isFinal", I.IsFinal);
    io.map("isOverride", I.IsOverride);
    io.map("isExplicitObjectMemberFunction", I.IsExplicitObjectMemberFunction);
    if (I.Constexpr != ConstexprKind::None)
    {
        io.map("constexprKind", I.Constexpr);
    }
    if (I.StorageClass != StorageClassKind::None)
    {
        io.map("storageClass", I.StorageClass);
    }
    if (I.RefQualifier != ReferenceKind::None)
    {
        io.map("refQualifier", I.RefQualifier);
    }
    io.map("functionClass", I.Class);
    io.map("params", dom::LazyArray(I.Params, domCorpus));
    io.map("return", I.ReturnType);
    io.map("template", I.Template);
    io.map("overloadedOperator", I.OverloadedOperator);
    io.map("exceptionSpec", I.Noexcept);
    io.map("explicitSpec", I.Explicit);
    if (!I.Requires.Written.empty())
    {
        io.map("requires", I.Requires.Written);
    }
}

/** Map the FunctionInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FunctionInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Determine if one function would override the other
 */
MRDOCS_DECL
bool
overrides(FunctionInfo const& base, FunctionInfo const& derived);

} // clang::mrdocs

#endif
