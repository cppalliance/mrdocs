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
#include <mrdocs/Support/Describe.hpp>
#include <string_view>

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

MRDOCS_DESCRIBE_ENUM(
    AutoKind,
    Auto, DecltypeAuto)

/** Convert an auto-kind to its spelling.

    Custom (not the generic described-enum name): renders the written keyword
    `auto` / `decltype(auto)`, used when building type strings.

    @param kind The auto kind.
    @return String naming the keyword.
*/
constexpr
std::string_view
toString(AutoKind kind) noexcept
{
    switch (kind)
    {
    case AutoKind::Auto:         return "auto";
    case AutoKind::DecltypeAuto: return "decltype(auto)";
    default:                     return "";
    }
}

} // mrdocs

#endif
