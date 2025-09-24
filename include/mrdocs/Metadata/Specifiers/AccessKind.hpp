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

#ifndef MRDOCS_API_METADATA_SPECIFIERS_ACCESSKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_ACCESSKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

/** Access specifier.

    None is set to zero since it is the most
    frequently occurring access, and it is
    elided by the bitstream encoder because it
    has an all-zero bit pattern. This improves
    compression in the bitstream.

    None is used for namespace members and friend;
    such declarations have no access.
*/
enum class AccessKind
{
    /// Unspecified access
    None = 0,
    /// Public access
    Public,
    /// Protected access
    Protected,
    /// Private access
    Private,
};

MRDOCS_DECL
dom::String
toString(AccessKind kind) noexcept;

/** Return the AccessKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AccessKind const kind)
{
    v = toString(kind);
}

} // clang::mrdocs

#endif
