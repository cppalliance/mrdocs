//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_TYPE_HPP
#define MRDOX_API_METADATA_TYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

/** Metadata for naming a type.

    A TypeInfo object names a type.

    Builtin types:

    @li void
    @li std::nullptr_t
    @li bool
    @li char, unsigned char
    @li float, double
    @li short, int, long, long long
    @li unsigned short, unsignd int
    @li unsigned long, unsigned long long
*/
struct TypeInfo
{
    SymbolID id;
    std::string Name;

    explicit
    TypeInfo(
        SymbolID ID = SymbolID::zero,
        std::string_view name = "") noexcept
        : id(ID)
        , Name(name)
    {
    }

#if 0
    // VFALCO What was this for?
    bool operator==(TypeInfo const& Other) const
    {
        return Type == Other.Type;
    }
#endif
};

} // mrdox
} // clang

#endif
