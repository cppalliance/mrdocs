//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_SYMBOL_HPP
#define MRDOX_API_METADATA_SYMBOL_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Location.hpp>
#include <optional>

namespace clang {
namespace mrdox {

/** Base class for Info that have source locations.
*/
struct SymbolInfo
{
    /** Location where the entity was defined

        KRYSTIAN NOTE: this is used for entities which cannot be
        redeclared -- regardless of whether such a declaration is
        actually a definition (e.g. alias-declarations and
        typedef declarations are never definition).
    */
    std::optional<Location> DefLoc;

    /** Locations where the entity was declared,

        This does not include the definition.
    */
    std::vector<Location> Loc;

    //--------------------------------------------

    SymbolInfo() = default;
};

} // mrdox
} // clang

#endif
