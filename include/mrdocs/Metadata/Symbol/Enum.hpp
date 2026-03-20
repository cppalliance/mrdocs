//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_ENUM_HPP
#define MRDOCS_API_METADATA_SYMBOL_ENUM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Metadata for an enumeration declaration.
*/
struct EnumSymbol final
    : SymbolCommonBase<SymbolKind::Enum>
{
    /** Indicates whether this enum is scoped (e.g. enum class).

        If true, the enumerators are accessed with the scope resolution
        operator (e.g. EnumName::Enumerator).

        If false, the enumerators are accessed directly (e.g. Enumerator)
        in the parent context.
    */
    bool Scoped = false;

    /** The underlying type of this enum, if explicitly specified.

        If not specified, the underlying type is an implementation-defined
        integral type that can represent all the enumerator values defined in
        the enumeration.

        For `enum Foo : short { ... };` this will be represent `short`.
    */
    Optional<Polymorphic<Type>> UnderlyingType = std::nullopt;

    /** The members of this scope.

        All members are enum constants.

        Enum constants are independent symbol types that
        can be documented separately.
    */
    std::vector<SymbolID> Constants;

    //--------------------------------------------

    /** Construct an enum symbol with its ID.
    */
    explicit EnumSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    EnumSymbol,
    (Symbol),
    (Scoped, UnderlyingType, Constants)
)

/** Return the list of enum constants for this symbol.
*/
inline
auto&
allMembers(EnumSymbol const& T)
{
    return T.Constants;
}

/** Merge another EnumSymbol into this one.
    @param I Destination symbol to update.
    @param Other Source symbol providing data.
*/
MRDOCS_DECL
void
merge(EnumSymbol& I, EnumSymbol&& Other);

/** Map a EnumSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The EnumSymbol to map.
    @param domCorpus The DomCorpus used to create
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    EnumSymbol const& I,
    DomCorpus const* domCorpus);

/** Map the EnumSymbol to a @ref dom::Value object.

    @param v The output parameter to receive the dom::Value.
    @param I The EnumSymbol to convert.
    @param domCorpus The DomCorpus used to resolve references.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    EnumSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_ENUM_HPP
