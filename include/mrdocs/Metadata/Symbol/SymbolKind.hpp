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

#ifndef MRDOCS_API_METADATA_SYMBOL_SYMBOLKIND_HPP
#define MRDOCS_API_METADATA_SYMBOL_SYMBOLKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

/** Info variant discriminator
*/
enum class SymbolKind {
    /// Kind is not specified.
    None = 0,
    #define INFO(Type) Type,
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
};

/** Return the name of the SymbolKind as a string.
 */
MRDOCS_DECL
dom::String
toString(SymbolKind kind) noexcept;

/** Return the SymbolKind from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v, SymbolKind const kind)
{
    v = toString(kind);
}

consteval
std::underlying_type_t<SymbolKind>
countSymbolKind()
{
    std::underlying_type_t<SymbolKind> count = 0;
#define INFO(Type) count++;
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
    return count;
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_SYMBOLKIND_HPP
