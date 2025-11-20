//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TARG_TARGKIND_HPP
#define MRDOCS_API_METADATA_TARG_TARGKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string_view>

namespace mrdocs {

/** The kind of template argument.
*/
enum class TArgKind : int {
#define INFO(Type) Type,
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
};

/** Convert a template-argument kind to a string.
    @return String view naming the argument kind.
*/
MRDOCS_DECL
std::string_view
toString(TArgKind kind) noexcept;

/** Serialize a template-argument kind into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArgKind kind)
{
    v = toString(kind);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_TARG_TARGKIND_HPP
