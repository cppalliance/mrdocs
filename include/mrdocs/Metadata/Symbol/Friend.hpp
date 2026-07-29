//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_FRIEND_HPP
#define MRDOCS_API_METADATA_SYMBOL_FRIEND_HPP

#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/Reflection/MapReflectedType.hpp>
#include <vector>

namespace mrdocs {

/** Info for friend declarations.

    - Friendship is not transitive
    - Friendship is not inherited
    - Access specifiers do not affect the meaning of friend declarations

    The friends of a record are stored directly in the record's metadata.

    If the friend declaration is documented, the documentation is
    stored in the befriended symbol's metadata rather than in the
    relationship.
*/
struct FriendInfo final
{
    /** Befriended symbol.
    */
    SymbolID id = SymbolID::invalid;

    /** Befriended type.

        This member is nullable and only used when befriending a type.
    */
    Optional<Polymorphic<struct Type>> Type = std::nullopt;
};

MRDOCS_DESCRIBE_STRUCT(
    FriendInfo,
    (),
    (Type, id)
)

/** Map a FriendInfo to a dom::Object with deferred name lookup.
    @param io The IO object to map into.
    @param I The FriendInfo to map.
    @param domCorpus The DomCorpus context.
*/
template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    FriendInfo const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
    if (I.id)
    {
        io.defer("name", [&I, domCorpus]{ return dom::ValueFrom(I.id, domCorpus).get("name"); });
    }
}

/** Merge friend declarations, deduplicating by symbol ID.

    @param dst The destination.
    @param src The source (moved from).
*/
MRDOCS_DECL
void
merge(std::vector<FriendInfo>& dst, std::vector<FriendInfo>&& src);

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_FRIEND_HPP
