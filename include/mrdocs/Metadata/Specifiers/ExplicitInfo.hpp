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

#ifndef MRDOCS_API_METADATA_SPECIFIERS_EXPLICITINFO_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_EXPLICITINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Specifiers/ExplicitKind.hpp>
#include <string>

namespace clang::mrdocs {

/**
    Stores only the operand of the explicit-specifier or noexcept-specifier as a string.
    The complete expression is not preserved at this time.
    This is a temporary design and may be improved in the future.
*/
struct ExplicitInfo
{
    /** Whether an explicit-specifier was user-written.
    */
    bool Implicit = true;

    /** The evaluated exception specification.
    */
    ExplicitKind Kind = ExplicitKind::False;

    /** The operand of the explicit-specifier, if any.
    */
    std::string Operand;

    auto operator<=>(ExplicitInfo const&) const = default;
};

/** Convert ExplicitInfo to a string.

    @param info The explicit-specifier information.
    @param resolved If true, the operand is not shown when
    the explicit-specifier is non-dependent.
    @param implicit If true, implicit explicit-specifiers are shown.
    @return The string representation of the explicit-specifier.
*/
MRDOCS_DECL
dom::String
toString(
    ExplicitInfo const& info,
    bool resolved = false,
    bool implicit = false);

/** Return the ExplicitInfo as a @ref dom::Value string.

    @param v The output parameter to receive the dom::Value.
    @param I The ExplicitInfo to convert.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ExplicitInfo const& I)
{
    v = toString(I);
}

} // clang::mrdocs

#endif
