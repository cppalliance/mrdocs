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
#include <mrdocs/Metadata/Type/ArrayTypeInfo.hpp>
#include <mrdocs/Metadata/Type/AutoTypeInfo.hpp>
#include <mrdocs/Metadata/Type/DecltypeTypeInfo.hpp>
#include <mrdocs/Metadata/Type/FunctionTypeInfo.hpp>
#include <mrdocs/Metadata/Type/LValueReferenceTypeInfo.hpp>
#include <mrdocs/Metadata/Type/MemberPointerTypeInfo.hpp>
#include <mrdocs/Metadata/Type/NamedTypeInfo.hpp>
#include <mrdocs/Metadata/Type/PointerTypeInfo.hpp>
#include <mrdocs/Metadata/Type/RValueReferenceTypeInfo.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Support/TypeTraits.hpp>

namespace clang::mrdocs {

template<
    std::derived_from<TypeInfo> TypeTy,
    class F,
    class... Args>
decltype(auto)
visit(
    TypeTy& I,
    F&& f,
    Args&&... args)
{
    add_cv_from_t<TypeTy, TypeInfo>& II = I;
    switch(I.Kind)
    {
    case TypeKind::Named:
        return f(static_cast<add_cv_from_t<
            TypeTy, NamedTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Decltype:
        return f(static_cast<add_cv_from_t<
            TypeTy, DecltypeTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Auto:
        return f(static_cast<add_cv_from_t<
            TypeTy, AutoTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::LValueReference:
        return f(static_cast<add_cv_from_t<
            TypeTy, LValueReferenceTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::RValueReference:
        return f(static_cast<add_cv_from_t<
            TypeTy, RValueReferenceTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Pointer:
        return f(static_cast<add_cv_from_t<
            TypeTy, PointerTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::MemberPointer:
        return f(static_cast<add_cv_from_t<
            TypeTy, MemberPointerTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Array:
        return f(static_cast<add_cv_from_t<
            TypeTy, ArrayTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Function:
        return f(static_cast<add_cv_from_t<
            TypeTy, FunctionTypeInfo>&>(II),
                std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs);

inline
bool
operator==(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

inline std::strong_ordering
operator<=>(
    Optional<Polymorphic<TypeInfo>> const& lhs,
    Optional<Polymorphic<TypeInfo>> const& rhs)
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
    Optional<Polymorphic<TypeInfo>> const& lhs,
    Optional<Polymorphic<TypeInfo>> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

/** Return the inner type.

    The inner type is the type which is modified
    by a specifier (e.g. "int" in "pointer to int".
*/
MRDOCS_DECL
Optional<std::reference_wrapper<Polymorphic<TypeInfo> const>>
innerType(TypeInfo const& TI) noexcept;

/// @copydoc innerType(TypeInfo const&)
MRDOCS_DECL
Optional<std::reference_wrapper<Polymorphic<TypeInfo>>>
innerType(TypeInfo& TI) noexcept;

/// @copydoc innerType(TypeInfo const&)
MRDOCS_DECL
TypeInfo const*
innerTypePtr(TypeInfo const& TI) noexcept;

/// @copydoc innerTypePtr(TypeInfo const&)
MRDOCS_DECL
TypeInfo*
innerTypePtr(TypeInfo& TI) noexcept;

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
Polymorphic<TypeInfo> const&
innermostType(Polymorphic<TypeInfo> const& TI) noexcept;

/// @copydoc innermostType(Polymorphic<TypeInfo> const&)
MRDOCS_DECL
Polymorphic<TypeInfo>&
innermostType(Polymorphic<TypeInfo>& TI) noexcept;

// VFALCO maybe we should rename this to `renderType` or something?
MRDOCS_DECL
std::string
toString(
    TypeInfo const& T,
    std::string_view Name = "");

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<TypeInfo> const& I,
    DomCorpus const* domCorpus)
{
    if (I.valueless_after_move())
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<Polymorphic<TypeInfo>> const& I,
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


} // clang::mrdocs

#endif
