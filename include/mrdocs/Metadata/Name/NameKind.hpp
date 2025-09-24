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

namespace clang::mrdocs {

enum class NameKind
{
    /// A simple identifier
    Identifier = 1, // for bitstream
    /// A template instantiation
    Specialization
};

MRDOCS_DECL
dom::String
toString(NameKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NameKind const kind)
{
    v = toString(kind);
}

} // clang::mrdocs

#endif
