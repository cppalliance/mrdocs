//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAGRAPHBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAGRAPHBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A sequence of text nodes.

    Syntax:

    @code
    Plain paragraph text.

    Another paragraph.
    @endcode
*/
struct ParagraphBlock
    : BlockCommonBase<BlockKind::Paragraph>
    , InlineContainer
{
    /** Virtual destructor for the polymorphic block hierarchy.
    */
    ~ParagraphBlock() override = default;

    /** Construct an empty paragraph.
    */
    ParagraphBlock() noexcept = default;

};

MRDOCS_DESCRIBE_STRUCT(
    ParagraphBlock,
    (BlockCommonBase<BlockKind::Paragraph>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAGRAPHBLOCK_HPP
