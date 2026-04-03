//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BLOCKKIND_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BLOCKKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs::doc {

/** Enumerates the different kinds of DocComment blocks.
*/
enum class BlockKind {
    #define INFO(Type) Type,
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
};

MRDOCS_DESCRIBE_ENUM(
    BlockKind,
    Admonition, Brief, Code, Heading, Paragraph, List,
    DefinitionList, Quote, ThematicBreak, FootnoteDefinition, Table, Math,
    Param, Postcondition, Precondition, Returns, See, Throws, TParam)

/** Convert a block kind to its kebab-case string name.
*/
inline
dom::String
toString(BlockKind kind) noexcept
{
    switch (kind)
    {
        #define INFO(Type) case BlockKind::Type: return toKebabCase(#Type);
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
    }
    return "Unknown";
}

/** Return true if the given block kind represents a command block.
*/
constexpr
bool
isBlockCommand(BlockKind k) noexcept
{
    switch (k)
    {
        #define INFO(Type) \
        case BlockKind::Type: \
            return true;
#include <mrdocs/Metadata/DocComment/Block/BlockCommandNodes.inc>
#undef INFO
    default:
        return false;
    }
}

} // namespace mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BLOCKKIND_HPP
