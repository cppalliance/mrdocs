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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLECELL_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLECELL_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A cell in a table
*/
struct TableCell final
    : InlineContainer
{
    /** Order cells by their inline content.
    */
    auto operator<=>(TableCell const&) const = default;

    /** Equality compares inline content.
    */
    bool operator==(TableCell const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    TableCell,
    (InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLECELL_HPP
