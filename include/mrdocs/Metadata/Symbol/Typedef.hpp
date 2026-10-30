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

#ifndef MRDOCS_API_METADATA_SYMBOL_TYPEDEF_HPP
#define MRDOCS_API_METADATA_SYMBOL_TYPEDEF_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** Info for typedef and using declarations.
*/
struct TypedefSymbol final
    : SymbolCommonBase<SymbolKind::Typedef>
{
    /** The aliased type.
    */
    Polymorphic<struct Type> Type = Polymorphic<struct Type>(NamedType{});

    /** Indicates if this is a new C++ "using"-style typedef

        @code
        using MyVector = std::vector<int>
        @endcode

        False means it's a C-style typedef:

        @code
        typedef std::vector<int> MyVector;
        @endcode
    */
    bool IsUsing = false;

    /** Template information when the alias is templated.
    */
    Optional<TemplateInfo> Template;

    //--------------------------------------------

    /** Create a typedef symbol bound to an ID.
    */
    explicit TypedefSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare typedef symbols, including alias target and template.
    */
    MRDOCS_DECL
    std::strong_ordering
    operator<=>(TypedefSymbol const& other) const;

};

MRDOCS_DESCRIBE_STRUCT(
    TypedefSymbol,
    (SymbolCommonBase<SymbolKind::Typedef>),
    (Type, IsUsing, Template)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_TYPEDEF_HPP
