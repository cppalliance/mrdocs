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

#ifndef MRDOCS_API_METADATA_TYPE_TYPEBASE_HPP
#define MRDOCS_API_METADATA_TYPE_TYPEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Metadata/Type/TypeKind.hpp>
#include <mrdocs/Support/Reflection/CompareReflectedType.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <vector>

namespace mrdocs {

/* Forward declarations
 */
#define INFO(TypeKind) struct TypeKind##Type;
#include <mrdocs/Metadata/Type/TypeNodes.inc>

/** A possibly qualified type.

    This class represents a type that may have
    qualifiers (e.g. const, volatile).

    This base class is used to store the kind
    of type. Derived classes are used to store
    the type information according to the kind.
*/
struct Type {
    /** The kind of Type this is
    */
    TypeKind Kind;

    /** Whether this is the pattern of a pack expansion.
    */
    bool IsPackExpansion = false;

    /** The const qualifier
    */
    bool IsConst = false;

    /** The volatile qualifier
    */
    bool IsVolatile = false;

    /** The constraints associated with the type

        This represents the constraints associated with the type,
        such as SFINAE constraints.

        For instance, if SFINAE detection is enabled, the
        expression `std::enable_if_t<std::is_integral_v<T>, T>`
        will have type `T` (NamedType) and constraints
        `{std::is_integral_v<T>}`.
    */
    std::vector<ExprInfo> Constraints;

    /** Return the symbol named by this type.
    */
    SymbolID
    namedSymbol() const noexcept;

    /** View as const Type.
    */
    constexpr Type const& asType() const noexcept
    {
        return *this;
    }

    /** View as mutable Type.
    */
    constexpr Type& asType() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == TypeKind::Type; \
    }
#include <mrdocs/Metadata/Type/TypeNodes.inc>

#define INFO(TypeKindName) \
    constexpr TypeKindName##Type const& as##TypeKindName() const noexcept { \
        if (Kind == TypeKind::TypeKindName) \
            return reinterpret_cast<TypeKindName##Type const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Type/TypeNodes.inc>

#define INFO(TypeKindName) \
    constexpr TypeKindName##Type & as##TypeKindName() noexcept { \
        if (Kind == TypeKind::TypeKindName) \
            return reinterpret_cast<TypeKindName##Type&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Type/TypeNodes.inc>

#define INFO(TypeKindName) \
    constexpr TypeKindName##Type const* as##TypeKindName##Ptr() const noexcept { \
        if (Kind == TypeKind::TypeKindName) { return reinterpret_cast<TypeKindName##Type const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Type/TypeNodes.inc>

#define INFO(TypeKindName) \
    constexpr TypeKindName##Type * as##TypeKindName##Ptr() noexcept { \
        if (Kind == TypeKind::TypeKindName) { return reinterpret_cast<TypeKindName##Type *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Type/TypeNodes.inc>

protected:
    /** Virtual destructor for polymorphic base.
    */
    constexpr virtual ~Type() = default;

    /** Construct with a concrete type kind.
    */
    constexpr Type(
        TypeKind kind) noexcept
        : Kind(kind)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    Type,
    (),
    (Kind, IsPackExpansion, IsConst, IsVolatile, Constraints)
)

/** Serialize a Type into a DOM value.
*/
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Type const& I,
    DomCorpus const* domCorpus);

/** CRTP base that ties a concrete type to a fixed TypeKind.
*/
template<TypeKind K>
struct TypeCommonBase : Type {
    /** Static discriminator for the concrete type.
    */
    static constexpr TypeKind kind_id = K;

    /** True when this concrete kind is a named type.
        @return `true` if `kind_id` is `TypeKind::Named`.
    */
    static constexpr bool isNamed()           noexcept { return K == TypeKind::Named; }
    /** True when this concrete kind is decltype.
        @return `true` if `kind_id` is `TypeKind::Decltype`.
    */
    static constexpr bool isDecltype()        noexcept { return K == TypeKind::Decltype; }
    /** True when this concrete kind is auto.
        @return `true` if `kind_id` is `TypeKind::Auto`.
    */
    static constexpr bool isAuto()            noexcept { return K == TypeKind::Auto; }
    /** True when this is an lvalue reference.
        @return `true` if `kind_id` is `TypeKind::LValueReference`.
    */
    static constexpr bool isLValueReference() noexcept { return K == TypeKind::LValueReference; }
    /** True when this is an rvalue reference.
        @return `true` if `kind_id` is `TypeKind::RValueReference`.
    */
    static constexpr bool isRValueReference() noexcept { return K == TypeKind::RValueReference; }
    /** True when this is a pointer type.
        @return `true` if `kind_id` is `TypeKind::Pointer`.
    */
    static constexpr bool isPointer()         noexcept { return K == TypeKind::Pointer; }
    /** True when this is a member pointer.
        @return `true` if `kind_id` is `TypeKind::MemberPointer`.
    */
    static constexpr bool isMemberPointer()   noexcept { return K == TypeKind::MemberPointer; }
    /** True when this is an array type.
        @return `true` if `kind_id` is `TypeKind::Array`.
    */
    static constexpr bool isArray()           noexcept { return K == TypeKind::Array; }
    /** True when this is a function type.
        @return `true` if `kind_id` is `TypeKind::Function`.
    */
    static constexpr bool isFunction()        noexcept { return K == TypeKind::Function; }

    MRDOCS_DESCRIBE_CLASS(TypeCommonBase, (Type), ())

protected:
    /** Construct the base with the fixed kind.
    */
    constexpr TypeCommonBase() noexcept
        : Type(K)
    {
    }
};

/** Compare two polymorphic types by visitor dispatch.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Type> const&, Polymorphic<Type> const&);

/** Equality for two polymorphic types.
*/
inline bool
operator==(Polymorphic<Type> const& a, Polymorphic<Type> const& b)
{
    return std::is_eq(a <=> b);
}

} // mrdocs

#endif
