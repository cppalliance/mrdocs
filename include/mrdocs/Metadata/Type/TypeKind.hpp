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

enum class TypeKind
{
    #define INFO(Type) Type,
#include <mrdocs/Metadata/Type/TypeNodes.inc>
};

MRDOCS_DECL
dom::String
toString(TypeKind kind) noexcept;

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
