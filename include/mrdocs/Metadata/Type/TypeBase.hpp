//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_TYPEBASE_HPP
#define MRDOCS_API_METADATA_TYPE_TYPEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <mrdocs/Metadata/Type/TypeKind.hpp>
#include <vector>

namespace clang::mrdocs {

/* Forward declarations
 */
#define INFO(Type) struct Type##TypeInfo;
#include <mrdocs/Metadata/Type/TypeInfoNodes.inc>

/** A possibly qualified type.

    This class represents a type that may have
    qualifiers (e.g. const, volatile).

    This base class is used to store the kind
    of type. Derived classes are used to store
    the type information according to the kind.
 */
struct TypeInfo
{
    /** The kind of TypeInfo this is
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

    auto operator<=>(TypeInfo const&) const = default;

    constexpr TypeInfo const& asType() const noexcept
    {
        return *this;
    }

    constexpr TypeInfo& asType() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == TypeKind::Type; \
    }
#include <mrdocs/Metadata/Type/TypeInfoNodes.inc>

    #define INFO(Type) \
    constexpr Type##TypeInfo const& as##Type() const noexcept { \
        if (Kind == TypeKind::Type) \
            return reinterpret_cast<Type##TypeInfo const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Type/TypeInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TypeInfo & as##Type() noexcept { \
        if (Kind == TypeKind::Type) \
            return reinterpret_cast<Type##TypeInfo&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Type/TypeInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TypeInfo const* as##Type##Ptr() const noexcept { \
        if (Kind == TypeKind::Type) { return reinterpret_cast<Type##TypeInfo const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Type/TypeInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TypeInfo * as##Type##Ptr() noexcept { \
        if (Kind == TypeKind::Type) { return reinterpret_cast<Type##TypeInfo *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Type/TypeInfoNodes.inc>

protected:
    constexpr virtual ~TypeInfo() = default;

    constexpr
    TypeInfo(
        TypeKind kind) noexcept
        : Kind(kind)
    {
    }
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypeInfo const& I,
    DomCorpus const* domCorpus);

template<TypeKind K>
struct TypeInfoCommonBase : TypeInfo
{
    static constexpr TypeKind kind_id = K;

    static constexpr bool isNamed()           noexcept { return K == TypeKind::Named; }
    static constexpr bool isDecltype()        noexcept { return K == TypeKind::Decltype; }
    static constexpr bool isAuto()            noexcept { return K == TypeKind::Auto; }
    static constexpr bool isLValueReference() noexcept { return K == TypeKind::LValueReference; }
    static constexpr bool isRValueReference() noexcept { return K == TypeKind::RValueReference; }
    static constexpr bool isPointer()         noexcept { return K == TypeKind::Pointer; }
    static constexpr bool isMemberPointer()   noexcept { return K == TypeKind::MemberPointer; }
    static constexpr bool isArray()           noexcept { return K == TypeKind::Array; }
    static constexpr bool isFunction()        noexcept { return K == TypeKind::Function; }

    auto operator<=>(TypeInfoCommonBase const&) const = default;

protected:
    constexpr
    TypeInfoCommonBase() noexcept
        : TypeInfo(K)
    {
    }
};

} // clang::mrdocs

#endif
