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

#ifndef MRDOCS_API_METADATA_SPECIFIERS_CONSTEXPRKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_CONSTEXPRKIND_HPP

#include <mrdocs/Platform.hpp>
#include <string>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** `constexpr`/`consteval` specifier kinds

    [dcl.spec.general] p2: At most one of the `constexpr`, `consteval`,
    and `constinit` keywords shall appear in a decl-specifier-seq
*/
enum class ConstexprKind
{
    /// No `constexpr` or `consteval` specifier
    None = 0,
    /// The `constexpr` specifier
    Constexpr,
    /// The `consteval` specifier
    /// only valid for functions
    Consteval,
};

MRDOCS_DESCRIBE_ENUM(
    ConstexprKind,
    None, Constexpr, Consteval)
MRDOCS_DESCRIBE_ENUM_UNDEFINED(ConstexprKind, None)

} // mrdocs

#endif
