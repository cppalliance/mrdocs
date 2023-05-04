//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_ACCESS_HPP
#define MRDOX_METADATA_ACCESS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbols.hpp>

namespace clang {
namespace mrdox {

/** Access specifier.

    Public is set to zero since it is the most
    frequently occurring access, and it is
    elided by the bitstream encoder because it
    has an all-zero bit pattern. This improves
    compression in the bitstream.

    @note It is by design that there is no
    constant to represent "none."
*/
enum class Access
{
    Public = 0,
    Protected,
    Private
};

/** A reference to a symbol, and an access speciier.

    This is used in records to refer to nested
    elements with access control.
*/
struct RefWithAccess
{
    SymbolID id;
    Access access;
};

} // mrdox
} // clang

#endif
