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

#ifndef MRDOX_META_SYMBOLS_HPP
#define MRDOX_META_SYMBOLS_HPP

#include <mrdox/Platform.hpp>
#include <llvm/ADT/StringRef.h>
#include <array>

namespace clang {
namespace mrdox {

/** A unique identifier for a symbol.

    This is calculated as the SHA1 digest of the
    USR. A USRs is a string that provide an
    unambiguous reference to a symbol.
*/
using SymbolID = std::array<unsigned char, 20>;

/** The empty Symbol ID.

    This is used to avoid unnecessary constructions,
    and to identify the global namespace.
*/
constexpr SymbolID const EmptySID = SymbolID();

/** The ID of the global namespace.
*/
constexpr SymbolID const globalNamespaceID = EmptySID;

/** Info variant discriminator
*/
enum class InfoType
{
    IT_default,
    IT_namespace,
    IT_record,
    IT_function,
    IT_enum,
    IT_typedef
};

/** Return the result of comparing s0 to s1.

    This function returns true if the string
    s0 is less than the string s1. The comparison
    is first made without regard to case, unless
    the strings compare equal and then they
    are compared with lowercase letters coming
    before uppercase letters.
*/
std::strong_ordering
compareSymbolNames(
    llvm::StringRef symbolName0,
    llvm::StringRef symbolName1) noexcept;

} // mrdox
} // clang

#endif
