//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEKIND_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

/** The kind of a C++ attribute.

    This identifies the standard C++ attributes mrdocs treats
    specially, regardless of how they were spelled in the source.
    For example, `[[deprecated]]`, `[[gnu::deprecated]]`, and
    `__declspec(deprecated)` all share the deprecated kind.

    Attributes mrdocs does not recognize use the `Other` kind.

    @note Like @ref TypeKind, this enum is intentionally NOT registered
    with `MRDOCS_DESCRIBE_ENUM`: the reflection-driven XML writer would
    otherwise emit a redundant `<kind>` child into every attribute
    element. Code that needs a string form calls `toString` below.
*/
enum class AttributeKind {
#define INFO(PascalName) PascalName,
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
};

/** Convert an AttributeKind to its string representation.
*/
MRDOCS_DECL
dom::String
toString(AttributeKind kind) noexcept;

/** Map an AttributeKind into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AttributeKind kind)
{
    v = toString(kind);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEKIND_HPP
