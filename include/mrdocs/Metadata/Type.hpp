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

/** Compare two types for equality for the purposes of overload resolution.

    Top-level const and volatile are ignored and arrays decay to pointers,
    as they do for function parameters; the comparison recurses through
    references, pointers, member pointers, function types, and the
    template arguments of specializations. How two names are judged equal
    is up to the caller: by symbol id for extracted declarations, or by
    resolving what a documentation reference wrote.

    @param lhs One type
    @param rhs The other type
    @param sameName How two names are judged equal
    @return Whether the two types are equal for overload resolution
*/
MRDOCS_DECL
bool
isDecayedEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    NameEquality const& sameName);

/// @copydoc isDecayedEqual(Polymorphic<Type> const&, Polymorphic<Type> const&, NameEquality const&)
MRDOCS_DECL
bool
isDecayedEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs);

/** Compare two types for equality.

    Unlike @ref isDecayedEqual, cv-qualifiers are significant and arrays do
    not decay, as for a template argument or the pointee of a pointer.

    @param lhs One type
    @param rhs The other type
    @param sameName How two names are judged equal
    @return Whether the two types are equal
*/
MRDOCS_DECL
bool
isEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    NameEquality const& sameName);

/// @copydoc isEqual(Polymorphic<Type> const&, Polymorphic<Type> const&, NameEquality const&)
MRDOCS_DECL
bool
isEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs);


} // mrdocs

#endif
