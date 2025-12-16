//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_FRIEND_HPP
#define MRDOCS_API_METADATA_SYMBOL_FRIEND_HPP

#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>

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

MRDOCS_DECL
void
merge(FriendInfo& I, FriendInfo&& Other);

/** Map a FriendInfo to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The FriendInfo to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    FriendInfo const& I,
    DomCorpus const* domCorpus)
{
    if (I.id)
    {
        io.defer("name", [&I, domCorpus]{
            return dom::ValueFrom(I.id, domCorpus).get("name");
        });
        io.map("symbol", I.id);
    }
    else if (I.Type)
    {
        io.defer("name", [&]{
            return dom::ValueFrom(I.Type, domCorpus).get("name");
        });
        io.map("type", I.Type);
    }
}

/** Map the FriendInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FriendInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_FRIEND_HPP
