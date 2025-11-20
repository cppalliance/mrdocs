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

#ifndef MRDOCS_API_METADATA_SYMBOL_FUNCTION_HPP
#define MRDOCS_API_METADATA_SYMBOL_FUNCTION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Metadata/Symbol/FunctionClass.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** Metadata for a function or method.
*/
struct FunctionSymbol final
    : SymbolCommonBase<SymbolKind::Function>
{
    /** Info about the return type of this function.

        If the function has a deduced return type, this contains
        `auto` to indicate that.

        By default, we also use `auto` in the member to indicate
        an unknown return type.
    */
    Polymorphic<Type> ReturnType = Polymorphic<Type>(AutoType{});

    /// List of parameters.
    std::vector<Param> Params;

    /// When present, this function is a template or specialization.
    Optional<TemplateInfo> Template;

    /// The class of function this is
    FunctionClass Class = FunctionClass::Normal;

    /** Exception specification for the function.
    */
    NoexceptInfo Noexcept;
    /** Constrained requires-clause if present.
    */
    ExprInfo Requires;
    /** True when the function is variadic.
    */
    bool IsVariadic = false;
    /** True when this declaration is implicitly defaulted.
    */
    bool IsDefaulted = false;
    /** True when explicitly defaulted with `= default`.
    */
    bool IsExplicitlyDefaulted = false;
    /** True when this declaration is deleted.
    */
    bool IsDeleted = false;
    /** True when deleted as written (vs deduced).
    */
    bool IsDeletedAsWritten = false;
    /** True when marked [[noreturn]] or equivalent.
    */
    bool IsNoReturn = false;
    /** True when annotated with override.
    */
    bool HasOverrideAttr = false;
    /** True when using a trailing return type.
    */
    bool HasTrailingReturn = false;
    /** True when declared [[nodiscard]].
    */
    bool IsNodiscard = false;
    /** True when explicit object parameter syntax is used.
    */
    bool IsExplicitObjectMemberFunction = false;
    /** constexpr/consteval specifier.
    */
    ConstexprKind Constexpr = ConstexprKind::None;
    /** Overloaded operator kind, if any.
    */
    OperatorKind OverloadedOperator = OperatorKind::None;
    /** Storage class specifier.
    */
    StorageClassKind StorageClass = StorageClassKind::None;
    /** Collected attributes attached to the declaration.
    */
    std::vector<std::string> Attributes;

    // CXXMethodDecl
    /** True when this is a non-static member function.
    */
    bool IsRecordMethod = false;
    /** True when declared virtual (after overrides).
    */
    bool IsVirtual = false;
    /** True when explicitly written virtual.
    */
    bool IsVirtualAsWritten = false;
    /** True when the function is pure virtual.
    */
    bool IsPure = false;
    /** True when qualified const.
    */
    bool IsConst = false;
    /** True when qualified volatile.
    */
    bool IsVolatile = false;
    /** True when final-qualified.
    */
    bool IsFinal = false;
    /** Reference qualifier on the member function, if any.
    */
    ReferenceKind RefQualifier = ReferenceKind::None;
    /** explicit-specifier information.
    */
    ExplicitInfo Explicit;

    //--------------------------------------------

    /** Construct a function symbol with its ID.
    */
    explicit FunctionSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare functions by signature, qualifiers, and metadata.
    */
    std::strong_ordering
    operator<=>(FunctionSymbol const& other) const;
};

/** Merge metadata from another function symbol.
    @param I Destination symbol to update.
    @param Other Source symbol providing additional data.
*/
MRDOCS_DECL
void
merge(FunctionSymbol& I, FunctionSymbol&& Other);

/** Map a FunctionSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The FunctionSymbol to map.
    @param domCorpus The DomCorpus used to create
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    FunctionSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("isVariadic", I.IsVariadic);
    io.map("isVirtual", I.IsVirtual);
    io.map("isVirtualAsWritten", I.IsVirtualAsWritten);
    io.map("isPure", I.IsPure);
    io.map("isDefaulted", I.IsDefaulted);
    io.map("isExplicitlyDefaulted", I.IsExplicitlyDefaulted);
    io.map("isDeleted", I.IsDeleted);
    io.map("isDeletedAsWritten", I.IsDeletedAsWritten);
    io.map("isNoReturn", I.IsNoReturn);
    io.map("hasOverrideAttr", I.HasOverrideAttr);
    io.map("hasTrailingReturn", I.HasTrailingReturn);
    io.map("isConst", I.IsConst);
    io.map("isVolatile", I.IsVolatile);
    io.map("isFinal", I.IsFinal);
    io.map("isNodiscard", I.IsNodiscard);
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
    io.map("attributes", dom::LazyArray(I.Attributes));
}

/** Map the FunctionSymbol to a @ref dom::Value object.

    @param v The output parameter to receive the dom::Value.
    @param I The FunctionSymbol to convert.
    @param domCorpus The DomCorpus used to resolve references.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FunctionSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Determine if one function would override the other

    @param base The base function
    @param derived The derived function
*/
MRDOCS_DECL
bool
overrides(FunctionSymbol const& base, FunctionSymbol const& derived);

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_FUNCTION_HPP
