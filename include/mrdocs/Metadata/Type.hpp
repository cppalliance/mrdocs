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

#include <string>
#include <vector>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/TypeTraits.hpp>

namespace clang::mrdocs {

std::strong_ordering
operator<=>(Polymorphic<NameInfo> const& lhs, Polymorphic<NameInfo> const& rhs);

std::strong_ordering
operator<=>(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs);

enum QualifierKind
{
    None,
    Const,
    Volatile
};

MRDOCS_DECL dom::String toString(QualifierKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    QualifierKind kind)
{
    v = toString(kind);
}

enum class TypeKind
{
    Named = 1, // for bitstream
    Decltype,
    Auto,
    LValueReference,
    RValueReference,
    Pointer,
    MemberPointer,
    Array,
    Function
};

MRDOCS_DECL dom::String toString(TypeKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypeKind kind)
{
    v = toString(kind);
}

enum class AutoKind
{
    Auto,
    DecltypeAuto
};

MRDOCS_DECL dom::String toString(AutoKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AutoKind kind)
{
    v = toString(kind);
}

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

    /** The constraints associated with the type

        This represents the constraints associated with the type,
        such as SFINAE constraints.

        For instance, if SFINAE detection is enabled, the
        expression `std::enable_if_t<std::is_integral_v<T>, T>`
        will have type `T` (NamedType) and constraints
        `{std::is_integral_v<T>}`.
     */
    std::vector<ExprInfo> Constraints;

    constexpr virtual ~TypeInfo() = default;

    constexpr bool isNamed()           const noexcept { return Kind == TypeKind::Named; }
    constexpr bool isDecltype()        const noexcept { return Kind == TypeKind::Decltype; }
    constexpr bool isAuto()            const noexcept { return Kind == TypeKind::Auto; }
    constexpr bool isLValueReference() const noexcept { return Kind == TypeKind::LValueReference; }
    constexpr bool isRValueReference() const noexcept { return Kind == TypeKind::RValueReference; }
    constexpr bool isPointer()         const noexcept { return Kind == TypeKind::Pointer; }
    constexpr bool isMemberPointer()   const noexcept { return Kind == TypeKind::MemberPointer; }
    constexpr bool isArray()           const noexcept { return Kind == TypeKind::Array; }
    constexpr bool isFunction()        const noexcept { return Kind == TypeKind::Function; }

    /** Return the inner type.

        The inner type is the type which is modified
        by a specifier (e.g. "int" in "pointer to int".
    */
    virtual
    TypeInfo*
    innerType() noexcept
    {
        return nullptr;
    }

    virtual
    TypeInfo const*
    cInnerType() const noexcept
    {
        return const_cast<TypeInfo*>(this)->innerType();
    }

    /** Return the symbol named by this type.
    */
    SymbolID
    namedSymbol() const noexcept;

    auto operator<=>(TypeInfo const&) const = default;

protected:
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

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<TypeInfo> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

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

struct NamedTypeInfo final
    : TypeInfoCommonBase<TypeKind::Named>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    Polymorphic<NameInfo> Name;

    std::strong_ordering
    operator<=>(NamedTypeInfo const& other) const;
};

struct DecltypeTypeInfo final
    : TypeInfoCommonBase<TypeKind::Decltype>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    ExprInfo Operand;

    auto operator<=>(DecltypeTypeInfo const&) const = default;
};

struct AutoTypeInfo final
    : TypeInfoCommonBase<TypeKind::Auto>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    AutoKind Keyword = AutoKind::Auto;
    Polymorphic<NameInfo> Constraint;

    std::strong_ordering
    operator<=>(AutoTypeInfo const&) const;
};

struct LValueReferenceTypeInfo final
    : TypeInfoCommonBase<TypeKind::LValueReference>
{
    Polymorphic<TypeInfo> PointeeType;

    TypeInfo*
    innerType() noexcept override
    {
        return PointeeType.operator->();
    }

    std::strong_ordering
    operator<=>(LValueReferenceTypeInfo const&) const;
};

struct RValueReferenceTypeInfo final
    : TypeInfoCommonBase<TypeKind::RValueReference>
{
    Polymorphic<TypeInfo> PointeeType;

    TypeInfo*
    innerType() noexcept override
    {
        return PointeeType.operator->();
    }

    std::strong_ordering
    operator<=>(RValueReferenceTypeInfo const&) const;
};

struct PointerTypeInfo final
    : TypeInfoCommonBase<TypeKind::Pointer>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    Polymorphic<TypeInfo> PointeeType;

    TypeInfo*
    innerType() noexcept override
    {
        return PointeeType.operator->();
    }

    std::strong_ordering
    operator<=>(PointerTypeInfo const&) const;
};

struct MemberPointerTypeInfo final
    : TypeInfoCommonBase<TypeKind::MemberPointer>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    Polymorphic<TypeInfo> ParentType;
    Polymorphic<TypeInfo> PointeeType;

    TypeInfo*
    innerType() noexcept override
    {
        return PointeeType.operator->();
    }


    std::strong_ordering
    operator<=>(MemberPointerTypeInfo const&) const;
};

struct ArrayTypeInfo final
    : TypeInfoCommonBase<TypeKind::Array>
{
    Polymorphic<TypeInfo> ElementType;
    ConstantExprInfo<std::uint64_t> Bounds;

    TypeInfo*
    innerType() noexcept override
    {
        return ElementType.operator->();
    }

    std::strong_ordering
    operator<=>(ArrayTypeInfo const&) const;
};

struct FunctionTypeInfo final
    : TypeInfoCommonBase<TypeKind::Function>
{
    Polymorphic<TypeInfo> ReturnType;
    std::vector<Polymorphic<TypeInfo>> ParamTypes;
    QualifierKind CVQualifiers = QualifierKind::None;
    ReferenceKind RefQualifier = ReferenceKind::None;
    NoexceptInfo ExceptionSpec;
    bool IsVariadic = false;

    TypeInfo*
    innerType() noexcept override
    {
        return ReturnType.operator->();
    }

    std::strong_ordering
    operator<=>(FunctionTypeInfo const&) const;
};

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


// VFALCO maybe we should rename this to `renderType` or something?
MRDOCS_DECL
std::string
toString(
    TypeInfo const& T,
    std::string_view Name = "");

} // clang::mrdocs

#endif
