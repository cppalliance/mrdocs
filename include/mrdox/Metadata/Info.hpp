//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_INFO_HPP
#define MRDOX_API_METADATA_INFO_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/Reference.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>

namespace clang {
namespace mrdox {

/** Common properties of all symbols
*/
struct Info
{
    /** The unique identifier for this symbol.
    */
    SymbolID id = SymbolID::zero;

    /** Kind of declaration.
    */
    InfoKind Kind = InfoKind::Default;

    /** The unqualified name.
    */
    std::string Name;

    /** In-order List of parent namespaces.
    */
    std::vector<SymbolID> Namespace;

    /** The extracted javadoc for this declaration.
    */
    std::unique_ptr<Javadoc> javadoc;

    //--------------------------------------------

    virtual ~Info() = default;
    Info(Info const &Other) = delete;
    Info(Info&& Other) = default;

    explicit
    Info(
        InfoKind kind = InfoKind::Default,
        SymbolID ID = SymbolID::zero)
        : id(ID)
        , Kind(kind)
    {
    }

    //
    // Observers
    //

    MRDOX_DECL
    std::string
    extractName() const;

#if 0
    /** Return the fully qualified name.
    */
    MRDOX_DECL
    std::string&
    getFullyQualifiedName(
        std::string& temp) const;
#endif
    /** Return a string representing the symbol type.

        For example, "namespace", "class", et. al.
    */
    llvm::StringRef
    symbolType() const noexcept;
};

//------------------------------------------------

template<class T>
constexpr bool isNamespace(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, NamespaceInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Namespace);
}

template<class T>
constexpr bool isNamespace() noexcept
{
    return std::is_same_v<T, NamespaceInfo>;
}

template<class T>
constexpr bool isRecord(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, RecordInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Record);
}

template<class T>
constexpr bool isRecord() noexcept
{
    return std::is_same_v<T, RecordInfo>;
}

template<class T>
constexpr bool isFunction(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, FunctionInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Function);
}

template<class T>
constexpr bool isFunction() noexcept
{
    return std::is_same_v<T, FunctionInfo>;
}

template<class T>
constexpr bool isEnum(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, EnumInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Enum);
}

template<class T>
constexpr bool isEnum() noexcept
{
    return std::is_same_v<T, EnumInfo>;
}

template<class T>
constexpr bool isTypedef(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, TypedefInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Typedef);
}

template<class T>
constexpr bool isTypedef() noexcept
{
    return std::is_same_v<T, TypedefInfo>;
}

template<class T>
constexpr bool isVar(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, VarInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Var);
}

template<class T>
constexpr bool isVar() noexcept
{
    return std::is_same_v<T, VarInfo>;
}

template<class T>
constexpr bool isField(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, FieldInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Field);
}

template<class T>
constexpr bool isField() noexcept
{
    return std::is_same_v<T, FieldInfo>;
}

template<class T>
constexpr bool isSpecialization(T const& t) noexcept
{
    static_assert(std::derived_from<T, Info>);
    return std::is_same_v<T, SpecializationInfo> || (
        std::is_same_v<T, Info> && t.Kind == InfoKind::Specialization);
}

template<class T>
constexpr bool isSpecialization() noexcept
{
    return std::is_same_v<T, SpecializationInfo>;
}

/** Invoke a function object with metadata.

    @return The return value of the function, if any.

    @param f The function object to invoke.
*/
template<class F>
auto
visit(Info const& I, F&& f)
{
    switch(I.Kind)
    {
    case InfoKind::Namespace:
        return f(static_cast<NamespaceInfo const&>(I));
    case InfoKind::Record:
        return f(static_cast<RecordInfo const&>(I));
    case InfoKind::Function:
        return f(static_cast<FunctionInfo const&>(I));
    case InfoKind::Enum:
        return f(static_cast<EnumInfo const&>(I));
    case InfoKind::Typedef:
        return f(static_cast<TypedefInfo const&>(I));
    case InfoKind::Variable:
        return f(static_cast<VarInfo const&>(I));
    case InfoKind::Field:
        return f(static_cast<FieldInfo const&>(I));
    case InfoKind::Specialization:
        return f(static_cast<SpecializationInfo const&>(I));
    }
}

} // mrdox
} // clang

#endif
