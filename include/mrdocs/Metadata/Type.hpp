//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_HPP
#define MRDOCS_API_METADATA_TYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/ArrayType.hpp>
#include <mrdocs/Metadata/Type/AutoType.hpp>
#include <mrdocs/Metadata/Type/DecltypeType.hpp>
#include <mrdocs/Metadata/Type/FunctionType.hpp>
#include <mrdocs/Metadata/Type/LValueReferenceType.hpp>
#include <mrdocs/Metadata/Type/MemberPointerType.hpp>
#include <mrdocs/Metadata/Type/NamedType.hpp>
#include <mrdocs/Metadata/Type/PointerType.hpp>
#include <mrdocs/Metadata/Type/RValueReferenceType.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

// Register Type's concrete kinds for the generic visit
// (Support/Reflection/Describe.hpp).
#define INFO(X) MRDOCS_KIND_ENTRY(Type, X##Type)
MRDOCS_DESCRIBE_KINDS_BEGIN(Type)
#include <mrdocs/Metadata/Type/TypeNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Type)
#undef INFO


/** Return the inner type.

    The inner type is the type that is modified
    by a specifier (e.g. "int" in "pointer to int").
*/
MRDOCS_DECL
Optional<Polymorphic<Type> const&>
innerType(Type const& TI) noexcept;

/// @copydoc innerType(Type const&)
MRDOCS_DECL
Optional<Polymorphic<Type>&>
innerType(Type& TI) noexcept;

/// @copydoc innerType(Type const&)
MRDOCS_DECL
Type const*
innerTypePtr(Type const& TI) noexcept;

/// @copydoc innerTypePtr(Type const&)
MRDOCS_DECL
Type*
innerTypePtr(Type& TI) noexcept;

/** Return the innermost type.

    The innermost type is the type which is not
    modified by any specifiers (e.g. "int" in
    "pointer to const int").

    If the type has an inner type, we recursively
    call this function until we reach the innermost
    type. If the type has no inner type, we return
    the current type.
*/
MRDOCS_DECL
Polymorphic<Type> const&
innermostType(Polymorphic<Type> const& TI) noexcept;

/** Return the innermost type (mutable overload).
*/
MRDOCS_DECL
Polymorphic<Type>&
innermostType(Polymorphic<Type>& TI) noexcept;

/** Render a type to a human-readable string.
    @param T Type to render.
    @param Name Optional identifier to append.
    @return Text representation of the type.
*/
MRDOCS_DECL
std::string
toString(
    Type const& T,
    std::string_view Name = "");


} // mrdocs

#endif
