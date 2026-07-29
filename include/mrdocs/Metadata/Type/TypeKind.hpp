//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_TYPEKIND_HPP
#define MRDOCS_API_METADATA_TYPE_TYPEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string_view>

namespace mrdocs {

/** Variants of C++ types captured in metadata.

    @note `TypeKind` is described (see MRDOCS_DESCRIBE_ENUM below) for
    reflection, but the custom `toString` is deliberately kept: it renders
    `lvalue-reference` / `rvalue-reference`, whereas the generic described-enum
    `toString` would give the kebab name `l-value-reference` /
    `r-value-reference`. The custom spelling is what the DOM/Handlebars side and
    script `kind:` matching expect.
*/
enum class TypeKind {
#define INFO(Type) Type,
#include <mrdocs/Metadata/Type/TypeNodes.inc>
};

MRDOCS_DESCRIBE_ENUM_BEGIN(TypeKind)
#define INFO(Name) MRDOCS_ENUM_ENTRY(TypeKind, Name)
#include <mrdocs/Metadata/Type/TypeNodes.inc>
MRDOCS_DESCRIBE_ENUM_END(TypeKind)

/** Convert a TypeKind to its string representation.

    @param kind The type kind.
    @return The kind's string representation.
*/
MRDOCS_DECL
std::string_view
toString(TypeKind kind) noexcept;

} // mrdocs

#endif
