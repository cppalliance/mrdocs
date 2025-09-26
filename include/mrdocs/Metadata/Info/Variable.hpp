//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_INFO_VARIABLE_HPP
#define MRDOCS_API_METADATA_INFO_VARIABLE_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

/** A variable.

    This includes variables at namespace
    or record scope.
*/
struct VariableInfo final
    : InfoCommonBase<InfoKind::Variable>
{
    /** The type of the variable */
    Polymorphic<TypeInfo> Type = Polymorphic<TypeInfo>(NamedTypeInfo{});

    /** The template information, if any.
     */
    Optional<TemplateInfo> Template;

    /** The default member initializer, if any.
     */
    ExprInfo Initializer;

    StorageClassKind StorageClass = StorageClassKind::None;

    bool IsInline = false;

    bool IsConstexpr = false;

    bool IsConstinit = false;

    bool IsThreadLocal = false;

    std::vector<std::string> Attributes;

    bool IsMaybeUnused = false;

    bool IsDeprecated = false;

    bool HasNoUniqueAddress = false;

    //--------------------------------------------
    // Record fields
    bool IsRecordField = false;

    /** Whether the field is declared mutable */
    bool IsMutable = false;

    /** Whether the field is a variant member */
    bool IsVariant = false;

    /** Whether the field is a bitfield */
    bool IsBitfield = false;

    /** The width of the bitfield */
    ConstantExprInfo<std::uint64_t> BitfieldWidth;

    //--------------------------------------------

    explicit VariableInfo(SymbolID const &ID) noexcept
        : InfoCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(VariableInfo const& other) const;
};

MRDOCS_DECL
void
merge(VariableInfo& I, VariableInfo&& Other);

/** Map a VariableInfo to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The VariableInfo to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    VariableInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("type", I.Type);
    io.map("template", I.Template);
    if (I.StorageClass != StorageClassKind::None)
    {
        io.map("storageClass", I.StorageClass);
    }
    io.map("isInline", I.IsInline);
    io.map("isConstexpr", I.IsConstexpr);
    io.map("isConstinit", I.IsConstinit);
    io.map("isThreadLocal", I.IsThreadLocal);
    if (!I.Initializer.Written.empty())
    {
        io.map("initializer", I.Initializer.Written);
    }
    io.map("attributes", dom::LazyArray(I.Attributes));
    io.map("isRecordField", I.IsRecordField);
    io.map("isMaybeUnused", I.IsMaybeUnused);
    io.map("isDeprecated", I.IsDeprecated);
    io.map("isVariant", I.IsVariant);
    io.map("isMutable", I.IsMutable);
    io.map("isBitfield", I.IsBitfield);
    if (I.IsBitfield)
    {
        io.map("bitfieldWidth", I.BitfieldWidth.Written);
    }
    io.map("hasNoUniqueAddress", I.HasNoUniqueAddress);
    io.map("attributes", dom::LazyArray(I.Attributes));
}

/** Map the VariableInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    VariableInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_VARIABLE_HPP
