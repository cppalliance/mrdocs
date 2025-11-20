//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
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
#include <mrdocs/Support/TypeTraits.hpp>

namespace mrdocs {

/** Visit a @ref Type with the provided callable.

    @param info The type instance to visit.
    @param fn The callable to dispatch to the concrete type.
    @param args Additional arguments forwarded to the callable.
    @return Whatever the callable returns.
*/
template<
    std::derived_from<Type> TypeTy,
    class F,
    class... Args>
decltype(auto)
visit(
    TypeTy& info,
    F&& fn,
    Args&&... args)
{
    add_cv_from_t<TypeTy, Type>& II = info;
    switch(info.Kind)
    {
    case TypeKind::Named:
        return fn(static_cast<add_cv_from_t<
            TypeTy, NamedType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Decltype:
        return fn(static_cast<add_cv_from_t<
            TypeTy, DecltypeType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Auto:
        return fn(static_cast<add_cv_from_t<
            TypeTy, AutoType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::LValueReference:
        return fn(static_cast<add_cv_from_t<
            TypeTy, LValueReferenceType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::RValueReference:
        return fn(static_cast<add_cv_from_t<
            TypeTy, RValueReferenceType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Pointer:
        return fn(static_cast<add_cv_from_t<
            TypeTy, PointerType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::MemberPointer:
        return fn(static_cast<add_cv_from_t<
            TypeTy, MemberPointerType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Array:
        return fn(static_cast<add_cv_from_t<
            TypeTy, ArrayType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Function:
        return fn(static_cast<add_cv_from_t<
            TypeTy, FunctionType>&>(II),
                std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Compare two polymorphic types for ordering.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Type> const& lhs, Polymorphic<Type> const& rhs);

/** Equality helper for polymorphic types.
*/
inline
bool
operator==(Polymorphic<Type> const& lhs, Polymorphic<Type> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

/** Compare optional polymorphic types, treating disengaged as less.
*/
inline std::strong_ordering
operator<=>(
    Optional<Polymorphic<Type>> const& lhs,
    Optional<Polymorphic<Type>> const& rhs)
{
    if (lhs && rhs)
    {
        return *lhs <=> *rhs;
    }
    if (!lhs && !rhs)
    {
        return std::strong_ordering::equal;
    }
    return bool(lhs) <=> bool(rhs);
}

/** Equality helper for optional polymorphic types.
*/
inline bool
operator==(
    Optional<Polymorphic<Type>> const& lhs,
    Optional<Polymorphic<Type>> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

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

/** Serialize a polymorphic type into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<Type> const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

/** Serialize an optional polymorphic type into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<Polymorphic<Type>> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    MRDOCS_ASSERT(!I->valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, **I, domCorpus);
}


} // mrdocs

#endif
