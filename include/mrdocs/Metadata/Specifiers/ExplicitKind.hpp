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

#ifndef MRDOCS_API_METADATA_SPECIFIERS_EXPLICITKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_EXPLICITKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom/String.hpp>
#include <string>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** Explicit specifier kinds
*/
enum class ExplicitKind
{
    /** No explicit-specifier or explicit-specifier evaluated to false
    */
    False = 0,
    /** explicit-specifier evaluates to true
    */
    True,
    /** Dependent explicit-specifier
    */
    Dependent
};

MRDOCS_DESCRIBE_ENUM(
    ExplicitKind,
    False, True, Dependent)
MRDOCS_DESCRIBE_ENUM_UNDEFINED(ExplicitKind, False)

} // mrdocs

#endif
