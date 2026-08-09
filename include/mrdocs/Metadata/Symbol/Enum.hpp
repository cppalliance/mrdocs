//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_ENUM_HPP
#define MRDOCS_API_METADATA_SYMBOL_ENUM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

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
    (SymbolCommonBase<SymbolKind::Enum>),
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


} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_ENUM_HPP
