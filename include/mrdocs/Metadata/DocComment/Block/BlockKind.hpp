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
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/String/String.hpp>

namespace mrdocs::doc {

/** Enumerates the different kinds of DocComment blocks.
*/
enum class BlockKind {
    #define INFO(Type) Type,
    #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
};

MRDOCS_DESCRIBE_ENUM_BEGIN(BlockKind)
#define INFO(Name) MRDOCS_ENUM_ENTRY(BlockKind, Name)
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
MRDOCS_DESCRIBE_ENUM_END(BlockKind)

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
