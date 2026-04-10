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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_QUOTEBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_QUOTEBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListItem.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>
#include <vector>

namespace mrdocs::doc {

/** A quoted block of text.

    Syntax:

    @code
    > quoted text
    @endcode

    Multi-line quotes:

    @code
    > This is the first line of a multi-line quote.
    > This is the second line.
    > And this is the third.
    @endcode

    Nested quotes:

    @code
    > This is the outer quote.
    >
    > > This is a nested quote within the outer quote.
    > >
    > > > This is a further nested quote.
    @endcode

    Quotes with other markdown elements:

    @code
    > ### Important Note
    >
    > - This blockquote contains a heading.
    > - And a list item.
    >
    > *Emphasis* and **strong emphasis** also work within blockquotes.
    @endcode
*/
struct QuoteBlock final
    : BlockCommonBase<BlockKind::Quote>
    , BlockContainer
{
};

MRDOCS_DESCRIBE_STRUCT(
    QuoteBlock,
    (BlockCommonBase<BlockKind::Quote>, BlockContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_QUOTEBLOCK_HPP
