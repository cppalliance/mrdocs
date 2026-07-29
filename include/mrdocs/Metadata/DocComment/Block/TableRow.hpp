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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEROW_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEROW_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableCell.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** An item in a list
*/
struct TableRow final
{
    /** True if this row represents a header.
    */
    bool is_header = false;
    /** Cells contained in the row.
    */
    std::vector<TableCell> Cells;

};

MRDOCS_DESCRIBE_STRUCT(
    TableRow,
    (),
    (is_header, Cells)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEROW_HPP
