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

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
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

    NoexceptInfo Noexcept;
    ExprInfo Requires;
    bool IsVariadic = false;
    bool IsDefaulted = false;
    bool IsExplicitlyDefaulted = false;
    bool IsDeleted = false;
    bool IsDeletedAsWritten = false;
    bool IsNoReturn = false;
    bool HasOverrideAttr = false;
    bool HasTrailingReturn = false;
    bool IsNodiscard = false;
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
    ReferenceKind RefQualifier = ReferenceKind::None;
    ExplicitInfo Explicit;

    //--------------------------------------------

    explicit FunctionSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(FunctionSymbol const& other) const;
};

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
