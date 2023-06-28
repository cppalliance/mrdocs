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
#include <mrdox/Metadata/Specifiers.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/MetadataFwd.hpp>
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

MRDOX_DECL
std::string_view
toString(QualifierKind kind);

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
    Function,
    Pack
};

MRDOX_DECL
std::string_view
toString(TypeKind kind);

struct TypeInfo
{
    TypeKind Kind;

    constexpr
    virtual
    ~TypeInfo() = default;

    constexpr bool isBuiltin()         const noexcept { return Kind == TypeKind::Builtin ; }
    constexpr bool isTag()             const noexcept { return Kind == TypeKind::Tag; }
    constexpr bool isSpecialization()  const noexcept { return Kind == TypeKind::Specialization; }
    constexpr bool isLValueReference() const noexcept { return Kind == TypeKind::LValueReference; }
    constexpr bool isRValueReference() const noexcept { return Kind == TypeKind::RValueReference; }
    constexpr bool isPointer()         const noexcept { return Kind == TypeKind::Pointer; }
    constexpr bool isMemberPointer()   const noexcept { return Kind == TypeKind::MemberPointer; }
    constexpr bool isArray()           const noexcept { return Kind == TypeKind::Array; }
    constexpr bool isFunction()        const noexcept { return Kind == TypeKind::Function; }
    constexpr bool isPack()            const noexcept { return Kind == TypeKind::Pack; }

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
    static constexpr bool isPack()            noexcept { return K == TypeKind::Pack; }

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
    std::vector<TArg> TemplateArgs;
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
    std::string BoundsValue;
    std::string BoundsExpr;
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

struct PackTypeInfo
    : IsType<TypeKind::Pack>
{
    std::unique_ptr<TypeInfo> PatternType;
};

template<typename F, typename... Args>
constexpr
auto
visit(
    TypeInfo& I,
    F&& f,
    Args&&... args)
{
    switch(I.Kind)
    {
    case TypeKind::Builtin:
        return f(static_cast<BuiltinTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Tag:
        return f(static_cast<TagTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Specialization:
        return f(static_cast<SpecializationTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::LValueReference:
        return f(static_cast<LValueReferenceTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::RValueReference:
        return f(static_cast<RValueReferenceTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Pointer:
        return f(static_cast<PointerTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::MemberPointer:
        return f(static_cast<MemberPointerTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Array:
        return f(static_cast<ArrayTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Function:
        return f(static_cast<FunctionTypeInfo&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Pack:
        return f(static_cast<PackTypeInfo&>(I),
            std::forward<Args>(args)...);
    default:
        MRDOX_UNREACHABLE();
    }
}

template<typename F, typename... Args>
constexpr
auto
visit(
    const TypeInfo& I,
    F&& f,
    Args&&... args)
{
    switch(I.Kind)
    {
    case TypeKind::Builtin:
        return f(static_cast<BuiltinTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Tag:
        return f(static_cast<TagTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Specialization:
        return f(static_cast<SpecializationTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::LValueReference:
        return f(static_cast<LValueReferenceTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::RValueReference:
        return f(static_cast<RValueReferenceTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Pointer:
        return f(static_cast<PointerTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::MemberPointer:
        return f(static_cast<MemberPointerTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Array:
        return f(static_cast<ArrayTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Function:
        return f(static_cast<FunctionTypeInfo const&>(I),
            std::forward<Args>(args)...);
    case TypeKind::Pack:
        return f(static_cast<PackTypeInfo const&>(I),
            std::forward<Args>(args)...);
    default:
        MRDOX_UNREACHABLE();
    }
}

MRDOX_DECL
std::string
toString(
    const TypeInfo& T,
    std::string_view Name = "");

} // mrdox
} // clang

#endif
