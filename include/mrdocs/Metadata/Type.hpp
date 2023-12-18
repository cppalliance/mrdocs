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
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdocs {

enum QualifierKind : int
{
    None,
    Const,
    Volatile
};

MRDOCS_DECL dom::String toString(QualifierKind kind) noexcept;

enum class TypeKind
{
    Named = 1, // for bitstream
    Decltype,
    LValueReference,
    RValueReference,
    Pointer,
    MemberPointer,
    Array,
    Function
};

MRDOCS_DECL dom::String toString(TypeKind kind) noexcept;

struct TypeInfo
{
    /** The kind of TypeInfo this is
    */
    TypeKind Kind;

    /** Whether this is the pattern of a pack expansion.
    */
    bool IsPackExpansion = false;

    constexpr virtual ~TypeInfo() = default;

    constexpr bool isNamed()           const noexcept { return Kind == TypeKind::Named; }
    constexpr bool isDecltype()        const noexcept { return Kind == TypeKind::Decltype; }
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
    virtual TypeInfo* innerType() const noexcept
    {
        return nullptr;
    }

    /** Return the symbol named by this type.
    */
    SymbolID namedSymbol() const noexcept;

protected:
    constexpr
    TypeInfo(
        TypeKind kind) noexcept
        : Kind(kind)
    {
    }
};

template<TypeKind K>
struct IsType : TypeInfo
{
    static constexpr TypeKind kind_id = K;

    static constexpr bool isNamed()           noexcept { return K == TypeKind::Named; }
    static constexpr bool isDecltype()        noexcept { return K == TypeKind::Decltype; }
    static constexpr bool isLValueReference() noexcept { return K == TypeKind::LValueReference; }
    static constexpr bool isRValueReference() noexcept { return K == TypeKind::RValueReference; }
    static constexpr bool isPointer()         noexcept { return K == TypeKind::Pointer; }
    static constexpr bool isMemberPointer()   noexcept { return K == TypeKind::MemberPointer; }
    static constexpr bool isArray()           noexcept { return K == TypeKind::Array; }
    static constexpr bool isFunction()        noexcept { return K == TypeKind::Function; }

protected:
    constexpr
    IsType() noexcept
        : TypeInfo(K)
    {
    }
};

struct NamedTypeInfo
    : IsType<TypeKind::Named>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<NameInfo> Name;
};

struct DecltypeTypeInfo
    : IsType<TypeKind::Decltype>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    ExprInfo Operand;
};

struct LValueReferenceTypeInfo
    : IsType<TypeKind::LValueReference>
{
    std::unique_ptr<TypeInfo> PointeeType;

    TypeInfo* innerType() const noexcept override
    {
        return PointeeType.get();
    }
};

struct RValueReferenceTypeInfo
    : IsType<TypeKind::RValueReference>
{
    std::unique_ptr<TypeInfo> PointeeType;

    TypeInfo* innerType() const noexcept override
    {
        return PointeeType.get();
    }
};

struct PointerTypeInfo
    : IsType<TypeKind::Pointer>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<TypeInfo> PointeeType;

    TypeInfo* innerType() const noexcept override
    {
        return PointeeType.get();
    }
};

struct MemberPointerTypeInfo
    : IsType<TypeKind::MemberPointer>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<TypeInfo> ParentType;
    std::unique_ptr<TypeInfo> PointeeType;

    TypeInfo* innerType() const noexcept override
    {
        return PointeeType.get();
    }
};

struct ArrayTypeInfo
    : IsType<TypeKind::Array>
{
    std::unique_ptr<TypeInfo> ElementType;
    ConstantExprInfo<std::uint64_t> Bounds;

    TypeInfo* innerType() const noexcept override
    {
        return ElementType.get();
    }
};

struct FunctionTypeInfo
    : IsType<TypeKind::Function>
{
    std::unique_ptr<TypeInfo> ReturnType;
    std::vector<std::unique_ptr<TypeInfo>> ParamTypes;
    QualifierKind CVQualifiers = QualifierKind::None;
    ReferenceKind RefQualifier = ReferenceKind::None;
    NoexceptInfo ExceptionSpec;

    TypeInfo* innerType() const noexcept override
    {
        return ReturnType.get();
    }
};

template<
    class TypeTy,
    class F,
    class... Args>
    requires std::derived_from<TypeTy, TypeInfo>
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

// VFALCO maybe we should rename this to `renderType` or something?
MRDOCS_DECL
std::string
toString(
    const TypeInfo& T,
    std::string_view Name = "");

} // mrdocs
} // clang

#endif
