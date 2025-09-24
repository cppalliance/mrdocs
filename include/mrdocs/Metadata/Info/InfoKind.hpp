//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_INFO_INFOKIND_HPP
#define MRDOCS_API_METADATA_INFO_INFOKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace clang::mrdocs {

/** Info variant discriminator
*/
enum class InfoKind
{
    /// Kind is not specified.
    None = 0,
    #define INFO(Type) Type,
#include <mrdocs/Metadata/Info/InfoNodes.inc>
};

/** Return the name of the InfoKind as a string.
 */
MRDOCS_DECL
dom::String
toString(InfoKind kind) noexcept;

/** Return the InfoKind from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    InfoKind const kind)
{
    v = toString(kind);
}

consteval
std::underlying_type_t<InfoKind>
countInfoKind()
{
    std::underlying_type_t<InfoKind> count = 0;
#define INFO(Type) count++;
#include <mrdocs/Metadata/Info/InfoNodes.inc>
    return count;
}

} // clang::mrdocs

#endif
