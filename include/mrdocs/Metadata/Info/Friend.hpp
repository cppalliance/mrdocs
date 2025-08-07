//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_FRIEND_HPP
#define MRDOCS_API_METADATA_FRIEND_HPP

#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

/** Info for friend declarations.

    - Friendship is not transitive
    - Friendship is not inherited
    - Access specifiers have no effect on the meaning of friend declarations
*/
struct FriendInfo final
{
    /** Befriended symbol.
    */
    SymbolID id = SymbolID::invalid;

    /** Befriended type.
    */
    Polymorphic<TypeInfo> Type = std::nullopt;
};

MRDOCS_DECL
void
merge(FriendInfo& I, FriendInfo&& Other);

/** Map a FriendInfo to a dom::Object.
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

} // clang::mrdocs

#endif
