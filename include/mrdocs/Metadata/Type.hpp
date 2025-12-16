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

template<
    std::derived_from<Type> TypeTy,
    class F,
    class... Args>
decltype(auto)
visit(
    TypeTy& I,
    F&& f,
    Args&&... args)
{
    add_cv_from_t<TypeTy, Type>& II = I;
    switch(I.Kind)
    {
    case TypeKind::Named:
        return f(static_cast<add_cv_from_t<
            TypeTy, NamedType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Decltype:
        return f(static_cast<add_cv_from_t<
            TypeTy, DecltypeType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Auto:
        return f(static_cast<add_cv_from_t<
            TypeTy, AutoType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::LValueReference:
        return f(static_cast<add_cv_from_t<
            TypeTy, LValueReferenceType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::RValueReference:
        return f(static_cast<add_cv_from_t<
            TypeTy, RValueReferenceType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Pointer:
        return f(static_cast<add_cv_from_t<
            TypeTy, PointerType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::MemberPointer:
        return f(static_cast<add_cv_from_t<
            TypeTy, MemberPointerType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Array:
        return f(static_cast<add_cv_from_t<
            TypeTy, ArrayType>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Function:
        return f(static_cast<add_cv_from_t<
            TypeTy, FunctionType>&>(II),
                std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Type> const& lhs, Polymorphic<Type> const& rhs);

inline
bool
operator==(Polymorphic<Type> const& lhs, Polymorphic<Type> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

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

/// @copydoc innermostType(Polymorphic<Type> const&)
MRDOCS_DECL
Polymorphic<Type>&
innermostType(Polymorphic<Type>& TI) noexcept;

// VFALCO maybe we should rename this to `renderType` or something?
MRDOCS_DECL
std::string
toString(Type const& T,
    std::string_view Name = "");

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
