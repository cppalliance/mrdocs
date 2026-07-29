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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_CODEBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_CODEBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Preformatted source code.
*/
struct CodeBlock final
    : BlockCommonBase<BlockKind::Code>
{
    /** Raw code text inside the fenced block.
    */
    std::string literal;

    /// Fence info string, e.g. "cpp"
    std::string info;

    /** Construct an empty code block.
    */
    CodeBlock() noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    CodeBlock,
    (BlockCommonBase<BlockKind::Code>),
    (literal, info)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_CODEBLOCK_HPP
