//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_AUTOKIND_HPP
#define MRDOCS_API_METADATA_TYPE_AUTOKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

/** The kind of `auto` keyword used in a declaration.

    This is either `auto` or `decltype(auto)`.
*/
enum class AutoKind
{
    /// The `auto` keyword
    Auto,
    /// The `decltype(auto)` keyword
    DecltypeAuto
};

MRDOCS_DECL
dom::String
toString(AutoKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AutoKind kind)
{
    v = toString(kind);
}

} // mrdocs

#endif
