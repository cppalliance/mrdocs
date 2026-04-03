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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_FOOTNOTEDEFINITIONBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_FOOTNOTEDEFINITIONBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Text defining the content of a footnote reference.

    Syntax:

    @code
    This is a sentence with a footnote[^1].

    [^1]: This is the content of the footnote. It can also have multiple paragraphs.
          Here is the second paragraph, which needs to be indented.
    @endcode
*/
struct FootnoteDefinitionBlock final
    : BlockCommonBase<BlockKind::FootnoteDefinition>
    , BlockContainer
{
    /** Footnote label identifier.
    */
    std::string label;

    /** Construct an empty footnote definition.
    */
    FootnoteDefinitionBlock() noexcept = default;
    /** Compare definitions by label and block content.
    */
    auto operator<=>(FootnoteDefinitionBlock const&) const = default;
    /** Equality compares label and block content.
    */
    bool operator==(FootnoteDefinitionBlock const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    FootnoteDefinitionBlock,
    (BlockCommonBase<BlockKind::FootnoteDefinition>, BlockContainer),
    (label)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_FOOTNOTEDEFINITIONBLOCK_HPP
