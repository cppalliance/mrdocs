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
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <array>
#include <memory>
#include <string>
#include <vector>

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

    constexpr bool isDefault() const noexcept { return Kind == InfoKind::Default; }
    constexpr bool isNamespace() const noexcept { return Kind == InfoKind::Namespace; }
    constexpr bool isRecord() const noexcept { return Kind == InfoKind::Record; }
    constexpr bool isFunction() const noexcept { return Kind == InfoKind::Function; }
    constexpr bool isEnum() const noexcept { return Kind == InfoKind::Enum; }
    constexpr bool isTypedef() const noexcept { return Kind == InfoKind::Typedef; }
    constexpr bool isVariable() const noexcept { return Kind == InfoKind::Variable; }
    constexpr bool isField() const noexcept { return Kind == InfoKind::Field; }
    constexpr bool isSpecialization() { return Kind == InfoKind::Specialization; }
};

/** InfoKind checking functions for derived `Info` types

    Used to optimize `InfoKind` checks through an `Info` derived class.
*/
template<InfoKind K>
struct IsInfo : Info
{
    static constexpr InfoKind kind_id = K;

    static constexpr bool isDefault() noexcept { return K== InfoKind::Default; }
    static constexpr bool isNamespace() noexcept { return K == InfoKind::Namespace; }
    static constexpr bool isRecord() noexcept { return K == InfoKind::Record; }
    static constexpr bool isFunction() noexcept { return K == InfoKind::Function; }
    static constexpr bool isEnum() noexcept { return K == InfoKind::Enum; }
    static constexpr bool isTypedef() noexcept { return K == InfoKind::Typedef; }
    static constexpr bool isVariable() noexcept { return K == InfoKind::Variable; }
    static constexpr bool isField() noexcept { return K == InfoKind::Field; }
    static constexpr bool isSpecialization() noexcept { return K == InfoKind::Specialization; }

    constexpr
    IsInfo()
        : Info(K)
    {
    }

    constexpr
    explicit
    IsInfo(SymbolID ID)
        : Info(K, ID)
    {
    }
};

} // mrdox
} // clang

#endif
