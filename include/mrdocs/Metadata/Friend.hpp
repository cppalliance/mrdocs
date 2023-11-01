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

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <utility>

namespace clang {
namespace mrdocs {

/** Info for friend declarations.
*/
struct FriendInfo
    : IsInfo<InfoKind::Friend>
    , SourceInfo
{
    /** Befriended symbol.
    */
    SymbolID FriendSymbol = SymbolID::invalid;

    /** Befriended type.
    */
    std::unique_ptr<TypeInfo> FriendType;

    //--------------------------------------------

    explicit FriendInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
