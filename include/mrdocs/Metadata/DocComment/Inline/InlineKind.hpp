//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEKIND_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs::doc {

/** Enumerates the different kinds of inline nodes.
*/
enum class InlineKind {
#define INFO(Type) Type,
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
};

/** Convert an inline kind to its kebab-case string.
*/
inline
dom::String
toString(InlineKind kind) noexcept
{
    switch (kind)
    {
        #define INFO(Type) case InlineKind::Type: return toKebabCase(#Type);
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
    }
    return "Unknown";
}

/** Serialize an inline kind into a DOM value.
    @param v Destination DOM value.
    @param kind Kind to serialize.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    InlineKind const kind)
{
    v = toString(kind);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEKIND_HPP
