//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_TYPE_HPP
#define MRDOX_API_METADATA_TYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Dom.hpp>
#include <mrdox/Metadata/Expression.hpp>
#include <mrdox/Metadata/Specifiers.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Support/TypeTraits.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdox {

enum QualifierKind : int
{
    None,
    Const,
    Volatile
};

MRDOX_DECL dom::String toString(QualifierKind kind) noexcept;

enum class TypeKind
{
    Builtin = 1, // for bitstream
    Tag,
    Specialization,
    LValueReference,
    RValueReference,
    Pointer,
    MemberPointer,
    Array,
    Function
};

MRDOX_DECL dom::String toString(TypeKind kind) noexcept;

struct TypeInfo
{
    /** The kind of TypeInfo this is
    */
    TypeKind Kind;

    /** Whether this is the pattern of a pack expansion.
    */
    bool IsPackExpansion = false;

    constexpr virtual ~TypeInfo() = default;

    constexpr bool isBuiltin()         const noexcept { return Kind == TypeKind::Builtin ; }
    constexpr bool isTag()             const noexcept { return Kind == TypeKind::Tag; }
    constexpr bool isSpecialization()  const noexcept { return Kind == TypeKind::Specialization; }
    constexpr bool isLValueReference() const noexcept { return Kind == TypeKind::LValueReference; }
    constexpr bool isRValueReference() const noexcept { return Kind == TypeKind::RValueReference; }
    constexpr bool isPointer()         const noexcept { return Kind == TypeKind::Pointer; }
    constexpr bool isMemberPointer()   const noexcept { return Kind == TypeKind::MemberPointer; }
    constexpr bool isArray()           const noexcept { return Kind == TypeKind::Array; }
    constexpr bool isFunction()        const noexcept { return Kind == TypeKind::Function; }

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

    static constexpr bool isBuiltin()         noexcept { return K == TypeKind::Builtin; }
    static constexpr bool isTag()             noexcept { return K == TypeKind::Tag; }
    static constexpr bool isSpecialization()  noexcept { return K == TypeKind::Specialization; }
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

struct BuiltinTypeInfo
    : IsType<TypeKind::Builtin>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::string Name;
};

struct TagTypeInfo
    : IsType<TypeKind::Tag>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<TypeInfo> ParentType;
    std::string Name;
    SymbolID id = SymbolID::zero;
};

struct SpecializationTypeInfo
    : IsType<TypeKind::Specialization>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<TypeInfo> ParentType;
    std::string Name;
    SymbolID id = SymbolID::zero;
    std::vector<std::unique_ptr<TArg>> TemplateArgs;
};

struct LValueReferenceTypeInfo
    : IsType<TypeKind::LValueReference>
{
    std::unique_ptr<TypeInfo> PointeeType;
};

struct RValueReferenceTypeInfo
    : IsType<TypeKind::RValueReference>
{
    std::unique_ptr<TypeInfo> PointeeType;
};

struct PointerTypeInfo
    : IsType<TypeKind::Pointer>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<TypeInfo> PointeeType;
};

struct MemberPointerTypeInfo
    : IsType<TypeKind::MemberPointer>
{
    QualifierKind CVQualifiers = QualifierKind::None;
    std::unique_ptr<TypeInfo> ParentType;
    std::unique_ptr<TypeInfo> PointeeType;
};

struct ArrayTypeInfo
    : IsType<TypeKind::Array>
{
    std::unique_ptr<TypeInfo> ElementType;
    ConstantExprInfo<std::uint64_t> Bounds;
};

struct FunctionTypeInfo
    : IsType<TypeKind::Function>
{
    std::unique_ptr<TypeInfo> ReturnType;
    std::vector<std::unique_ptr<TypeInfo>> ParamTypes;
    QualifierKind CVQualifiers = QualifierKind::None;
    ReferenceKind RefQualifier = ReferenceKind::None;
    NoexceptKind ExceptionSpec = NoexceptKind::None;
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
    case TypeKind::Builtin:
        return f(static_cast<add_cv_from_t<
            TypeTy, BuiltinTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Tag:
        return f(static_cast<add_cv_from_t<
            TypeTy, TagTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Specialization:
        return f(static_cast<add_cv_from_t<
            TypeTy, SpecializationTypeInfo>&>(II),
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
        MRDOX_UNREACHABLE();
    }
}

// VFALCO maybe we should rename this to `renderType` or something?
MRDOX_DECL
std::string
toString(
    const TypeInfo& T,
    std::string_view Name = "");

} // mrdox
} // clang

#endif
