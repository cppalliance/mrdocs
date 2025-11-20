//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_NAMEKIND_HPP
#define MRDOCS_API_METADATA_NAME_NAMEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

/** Kinds of names that appear in type and symbol metadata.
*/
enum class NameKind {
#define INFO(Type) Type,
#include <mrdocs/Metadata/Name/NameNodes.inc>
};

/** Convert a NameKind to its string form.
*/
MRDOCS_DECL
dom::String
toString(NameKind kind) noexcept;

/** Map a NameKind into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v, NameKind const kind)
{
    v = toString(kind);
}

} // mrdocs

#endif
