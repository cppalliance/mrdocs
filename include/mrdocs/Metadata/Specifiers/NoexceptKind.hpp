//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIFIERS_NOEXCEPTKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_NOEXCEPTKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom/String.hpp>
#include <string>

namespace clang::mrdocs {

/** Exception specification kinds
*/
enum class NoexceptKind
{
    /** Potentially-throwing exception specification
    */
    False = 0,
    /** Non-throwing exception specification
    */
    True,
    /** Dependent exception specification
    */
    Dependent
};

MRDOCS_DECL
dom::String
toString(NoexceptKind kind) noexcept;

} // clang::mrdocs

#endif
