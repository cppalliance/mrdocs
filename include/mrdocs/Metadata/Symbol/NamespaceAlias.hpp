//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_NAMESPACEALIAS_HPP
#define MRDOCS_API_METADATA_SYMBOL_NAMESPACEALIAS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>

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

    explicit NamespaceAliasSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void
merge(NamespaceAliasSymbol& I, NamespaceAliasSymbol&& Other);

/** Map a NamespaceAliasSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The NamespaceAliasSymbol to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    NamespaceAliasSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("aliasedSymbol", I.AliasedSymbol);
}

/** Map the NamespaceAliasSymbol to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NamespaceAliasSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_NAMESPACEALIAS_HPP
