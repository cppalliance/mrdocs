//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_TYPEKIND_HPP
#define MRDOCS_API_METADATA_TYPE_TYPEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

/** Variants of C++ types captured in metadata.

    @note `TypeKind` is intentionally NOT registered with
    `MRDOCS_DESCRIBE_ENUM`. Describing it would make the reflection-
    driven XML writer emit a redundant `<kind>...</kind>` child into
    every type element (NamedType, LValueReferenceType, ...), which
    would churn every XML golden test for no semantic gain. Code that
    needs a string form for a `TypeKind` value calls `toString` below;
    the script side of `mrdocs.set` falls back to `toString` for
    polymorphic `kind:` matching when the discriminator enum is
    undescribed, so script names (`lvalue-reference`, ...) match the
    DOM and Handlebars side and differ only from the XML writer's tag
    form (`l-value-reference`, ...).
*/
enum class TypeKind {
#define INFO(Type) Type,
#include <mrdocs/Metadata/Type/TypeNodes.inc>
};

/** Convert a TypeKind to its string representation.
*/
MRDOCS_DECL
dom::String
toString(TypeKind kind) noexcept;

/** Map a TypeKind into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypeKind kind)
{
    v = toString(kind);
}

} // mrdocs

#endif
