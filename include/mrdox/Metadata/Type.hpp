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

#ifndef MRDOX_METADATA_TYPE_HPP
#define MRDOX_METADATA_TYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Reference.hpp>

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
    /** The type being referenced.

        If this names a built-in type
    */
    Reference Type;

    //--------------------------------------------

    TypeInfo() = default;

    explicit
    TypeInfo(
        Reference const& R) noexcept
        : Type(R)
    {
    }

    // Convenience constructor for when there is no symbol ID or info type
    // (normally used for built-in types in tests).
    explicit
    TypeInfo(
        llvm::StringRef Name)
        : Type(
            SymbolID::zero, Name, InfoType::IT_default)
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
