//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTKIND_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

enum class ListKind {
    Unordered,
    Ordered
};

inline
dom::String
toString(ListKind kind) noexcept
{
    switch (kind)
    {
        case ListKind::Unordered:
            return "unordered";
        case ListKind::Ordered:
            return "ordered";
    }
    return "Unknown";
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v, ListKind const kind)
{
    v = toString(kind);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTKIND_HPP
