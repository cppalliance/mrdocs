//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_NAMESPACEALIAS_HPP
#define MRDOCS_API_METADATA_SYMBOL_NAMESPACEALIAS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** Info for namespace aliases.
*/
struct NamespaceAliasSymbol final
    : SymbolCommonBase<SymbolKind::NamespaceAlias>
{
    /** The aliased symbol.

        This is another namespace that might or might
        not be in the same project.
    */
    IdentifierName AliasedSymbol;

    //--------------------------------------------

    /** Create an alias symbol bound to an ID.
    */
    explicit NamespaceAliasSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    NamespaceAliasSymbol,
    (SymbolCommonBase<SymbolKind::NamespaceAlias>),
    (AliasedSymbol)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_NAMESPACEALIAS_HPP
