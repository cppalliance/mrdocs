//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAMESPACEALIAS_HPP
#define MRDOCS_API_METADATA_NAMESPACEALIAS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>

namespace clang::mrdocs {

/** Info for namespace aliases.
*/
struct NamespaceAliasInfo final
    : InfoCommonBase<InfoKind::NamespaceAlias>
{
    /** The aliased symbol.

        This is another namespace that might or might
        not be in the same project.
     */
    Polymorphic<NameInfo> AliasedSymbol;

    //--------------------------------------------

    explicit NamespaceAliasInfo(SymbolID const &ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void
merge(NamespaceAliasInfo& I, NamespaceAliasInfo&& Other);

/** Map a NamespaceAliasInfo to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The NamespaceAliasInfo to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    NamespaceAliasInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("aliasedSymbol", I.AliasedSymbol);
}

/** Map the NamespaceAliasInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NamespaceAliasInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
