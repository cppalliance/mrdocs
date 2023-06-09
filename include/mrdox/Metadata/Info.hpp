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
#include <mrdox/Metadata/Reference.hpp>
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

    constexpr bool isDefault() { return Kind == InfoKind::Default; }
    constexpr bool isNamespace() { return Kind == InfoKind::Namespace; }
    constexpr bool isRecord() { return Kind == InfoKind::Record; }
    constexpr bool isFunction() { return Kind == InfoKind::Function; }
    constexpr bool isEnum() { return Kind == InfoKind::Enum; }
    constexpr bool isTypedef() { return Kind == InfoKind::Typedef; }
    constexpr bool isVariable() { return Kind == InfoKind::Variable; }
    constexpr bool isField() { return Kind == InfoKind::Field; }
    constexpr bool isSpecialization() { return Kind == InfoKind::Specialization; }
};

/** InfoKind checking functions for derived `Info` types

    Used to optimize `InfoKind` checks through an `Info` derived class.
*/
template<InfoKind K>
struct IsInfo : Info
{
    static constexpr InfoKind kind_id = K;

    static constexpr bool isDefault() { return K== InfoKind::Default; }
    static constexpr bool isNamespace() { return K == InfoKind::Namespace; }
    static constexpr bool isRecord() { return K == InfoKind::Record; }
    static constexpr bool isFunction() { return K == InfoKind::Function; }
    static constexpr bool isEnum() { return K == InfoKind::Enum; }
    static constexpr bool isTypedef() { return K == InfoKind::Typedef; }
    static constexpr bool isVariable() { return K == InfoKind::Variable; }
    static constexpr bool isField() { return K == InfoKind::Field; }
    static constexpr bool isSpecialization() { return K == InfoKind::Specialization; }

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
