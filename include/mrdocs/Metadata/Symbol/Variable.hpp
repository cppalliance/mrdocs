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

#ifndef MRDOCS_API_METADATA_SYMBOL_VARIABLE_HPP
#define MRDOCS_API_METADATA_SYMBOL_VARIABLE_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace mrdocs {

/** A variable.

    This includes variables at namespace
    or record scope.
*/
struct VariableSymbol final
    : SymbolCommonBase<SymbolKind::Variable>
{
    /** The type of the variable
    */
    Polymorphic<struct Type> Type = Polymorphic<struct Type>(NamedType{});

    /** The template information, if any.
    */
    Optional<TemplateInfo> Template;

    /** The default member initializer, if any.
    */
    ExprInfo Initializer;

    /** Storage class specifier applied to the variable.
    */
    StorageClassKind StorageClass = StorageClassKind::None;

    /** Whether the variable is declared `inline`.
    */
    bool IsInline = false;

    /** Whether the variable is `constexpr`.
    */
    bool IsConstexpr = false;

    /** Whether the variable is `constinit`.
    */
    bool IsConstinit = false;

    /** Whether the variable is `thread_local`.
    */
    bool IsThreadLocal = false;

    /** Raw attribute spellings attached to the variable.
    */
    std::vector<std::string> Attributes;

    /** Whether the variable carries `[[maybe_unused]]`.
    */
    bool IsMaybeUnused = false;

    /** Whether the variable is marked deprecated.
    */
    bool IsDeprecated = false;

    /** Whether the variable uses [[no_unique_address]].
    */
    bool HasNoUniqueAddress = false;

    //--------------------------------------------
    // Record fields
    /** True if this variable is a data member of a record.
    */
    bool IsRecordField = false;

    /** Whether the field is declared mutable
    */
    bool IsMutable = false;

    /** Whether the field is a variant member
    */
    bool IsVariant = false;

    /** Whether the field is a bitfield
    */
    bool IsBitfield = false;

    /** The width of the bitfield
    */
    ConstantExprInfo<std::uint64_t> BitfieldWidth;

    //--------------------------------------------

    /** Create a variable symbol bound to an ID.
    */
    explicit VariableSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare variables by type, flags, and initializer.
    */
    std::strong_ordering
    operator<=>(VariableSymbol const& other) const;
};

/** Merge variable metadata, preserving existing values when set.
*/
MRDOCS_DECL
void
merge(VariableSymbol& I, VariableSymbol&& Other);

/** Map a VariableSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The VariableSymbol to map.
    @param domCorpus The DomCorpus used to create
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    VariableSymbol const& I,
    DomCorpus const* domCorpus);

/** Map the VariableSymbol to a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    VariableSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_VARIABLE_HPP
