//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEALIGNMENTKIND_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEALIGNMENTKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Horizontal alignment for table columns.
*/
enum class TableAlignmentKind {
    /// No explicit alignment; renderer default applies.
    None,
    /// Align content to the left edge.
    Left,
    /// Center the content.
    Center,
    /// Align content to the right edge.
    Right,
};

MRDOCS_DESCRIBE_ENUM(TableAlignmentKind, None, Left, Center, Right)

} // mrdocs

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEALIGNMENTKIND_HPP
