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

#ifndef MRDOCS_API_METADATA_SPECIFIERS_NOEXCEPTINFO_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_NOEXCEPTINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>
#include <mrdocs/Metadata/Specifiers/ConstexprKind.hpp>
#include <mrdocs/Metadata/Specifiers/ExplicitInfo.hpp>
#include <mrdocs/Metadata/Specifiers/ExplicitKind.hpp>
#include <mrdocs/Metadata/Specifiers/NoexceptKind.hpp>
#include <string>

namespace mrdocs {

// KRYSTIAN FIXME: this needs to be improved (a lot)
struct NoexceptInfo
{
    /** Whether a noexcept-specifier was user-written.
    */
    bool Implicit = true;

    /** The evaluated exception specification.
    */
    NoexceptKind Kind = NoexceptKind::False;

    /** The operand of the noexcept-specifier, if any.
    */
    std::string Operand;

    auto operator<=>(NoexceptInfo const&) const = default;
};

/** Convert NoexceptInfo to a string.

    @param info The noexcept-specifier information.

    @param resolved If true, the operand is not shown when
    the exception specification is non-dependent.

    @param implicit If true, implicit exception specifications
    are shown.

    @return The string representation of the noexcept-specifier.
*/
MRDOCS_DECL
dom::String
toString(
    NoexceptInfo const& info,
    bool resolved = false,
    bool implicit = false);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NoexceptInfo const& info)
{
    v = toString(info, false, false);
}

} // mrdocs

#endif
