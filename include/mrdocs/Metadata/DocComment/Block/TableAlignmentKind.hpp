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

namespace mrdocs {

enum class TableAlignmentKind {
    None,
    Left,
    Center,
    Right,
};

inline
dom::String
toString(TableAlignmentKind kind) noexcept
{
    switch (kind)
    {
        case TableAlignmentKind::None:
            return "none";
        case TableAlignmentKind::Left:
            return "left";
        case TableAlignmentKind::Center:
            return "center";
        case TableAlignmentKind::Right:
            return "right";
    }
    return "unknown";
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v, TableAlignmentKind const kind)
{
    v = toString(kind);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEALIGNMENTKIND_HPP
