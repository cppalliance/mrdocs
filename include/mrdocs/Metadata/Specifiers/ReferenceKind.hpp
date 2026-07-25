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

#ifndef MRDOCS_API_METADATA_SPECIFIERS_REFERENCEKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_REFERENCEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string_view>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Reference type kinds
*/
enum class ReferenceKind
{
    /// Not a reference
    None = 0,
    /// An L-Value reference
    LValue,
    /// An R-Value reference
    RValue
};

MRDOCS_DESCRIBE_ENUM(
    ReferenceKind,
    None, LValue, RValue)
MRDOCS_DESCRIBE_ENUM_UNDEFINED(ReferenceKind, None)

/** Return the written ref-qualifier syntax (`&`, `&&`, or empty).

    Customized rather than using the generic described-enum name, so the
    refQualifier field carries the source spelling.

    @param kind The reference kind.
    @return The ref-qualifier spelling; empty for ReferenceKind::None.
*/
constexpr
std::string_view
toString(ReferenceKind kind) noexcept
{
    switch (kind)
    {
    case ReferenceKind::None:   return "";
    case ReferenceKind::LValue: return "&";
    case ReferenceKind::RValue: return "&&";
    default:                    return "";
    }
}

} // mrdocs

#endif
