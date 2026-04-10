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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableAlignmentKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableRow.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>
#include <vector>

namespace mrdocs::doc {

/** A table block

    Syntax:

    @code
    | Header 1 | Header 2 | Header 3 |
    | :------- | :------: | -------: |
    | Left     | Center   | Right    |
    | Cell 1A  | Cell 1B  | Cell 1C  |
    | Cell 2A  | Cell 2B  | Cell 2C  |
    @endcode
*/
struct TableBlock final: BlockCommonBase<BlockKind::Table>
{
    /** Column alignments for each table column.
    */
    std::vector<TableAlignmentKind> Alignments;
    /** Rows that make up the table body (header first when present).
    */
    std::vector<TableRow> items;

};

MRDOCS_DESCRIBE_STRUCT(
    TableBlock,
    (BlockCommonBase<BlockKind::Table>),
    (Alignments, items)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEBLOCK_HPP
