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
*/
struct FriendInfo final
    : InfoCommonBase<InfoKind::Friend>
{
    /** Befriended symbol.
    */
    SymbolID FriendSymbol = SymbolID::invalid;

    /** Befriended type.
    */
    PolymorphicValue<TypeInfo> FriendType;

    //--------------------------------------------

    explicit FriendInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
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
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    if (I.FriendSymbol)
    {
        io.defer("name", [&I, domCorpus]{
            return dom::ValueFrom(I.FriendSymbol, domCorpus).get("name");
        });
        io.map("symbol", I.FriendSymbol);
    }
    else if (I.FriendType)
    {
        io.defer("name", [&]{
            return dom::ValueFrom(I.FriendType, domCorpus).get("name");
        });
        io.map("type", I.FriendType);
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
