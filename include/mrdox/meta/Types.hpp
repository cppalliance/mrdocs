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

#ifndef MRDOX_META_TYPES_HPP
#define MRDOX_META_TYPES_HPP

#include <array>

namespace clang {
namespace mrdox {

//------------------------------------------------

/*
    Definitions

    Metadata

    Symbol
        A string which identifies C++ language
        entities: namespace, function, variable,
        class, struct, union, member function,
        type alias,

    ----------------------------------------------

    NamespaceInfo
    FunctionInfo
    EnumInfo
    RecordInfo
    TypedefInfo
        (VarDecl?)

    namespace, function, enum,
    class, struct, union,
    typedef, type alias, variable

    ----------------------------------------------

    clang-check
        https://godbolt.org/z/3cbd3qG4Y
*/

//------------------------------------------------

/** A unique identifier for a symbol.

    This is calculated as the SHA1 digest of the USR.
    A USRs is a string that provide an unambiguous
    reference to a symbol.
*/
using SymbolID = std::array<unsigned char, 20>;

// Empty SymbolID for comparison, so we don't have to construct one every time.
inline SymbolID const EmptySID = SymbolID();

enum class InfoType
{
    IT_default,
    IT_namespace,
    IT_record,
    IT_function,
    IT_enum,
    IT_typedef
};

} // mrdox
} // clang

#endif
