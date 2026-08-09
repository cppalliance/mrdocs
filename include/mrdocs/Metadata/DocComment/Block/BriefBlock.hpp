//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BRIEFBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BRIEFBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** The brief description

    Syntax:

    @code
    @brief summary
    @endcode
*/
struct BriefBlock final
    : BlockCommonBase<BlockKind::Brief>
    , InlineContainer
{
    /** Names of declarations whose brief text was reused.
    */
    std::vector<std::string> copiedFrom;

    /** Create an empty brief.
    */
    BriefBlock() = default;

    /** Copy-construct from another brief.
    */
    BriefBlock(BriefBlock const& other) = default;

    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

    /** Copy-assign another brief.
    */
    BriefBlock&
    operator=(BriefBlock const& other) = default;

    /** Reuse inline container assignment operators.
    */
    using InlineContainer::operator=;

};

MRDOCS_DESCRIBE_STRUCT(
    BriefBlock,
    (BlockCommonBase<BlockKind::Brief>, InlineContainer),
    (copiedFrom)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BRIEFBLOCK_HPP
