//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_INFO_HPP
#define MRDOCS_API_METADATA_INFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdocs {

struct NamespaceInfo;
struct RecordInfo;
struct FunctionInfo;
struct EnumInfo;
struct FieldInfo;
struct TypedefInfo;
struct VariableInfo;
struct SpecializationInfo;

/** Info variant discriminator
*/
enum class InfoKind
{
    Namespace = 0,
    Record,
    Function,
    Enum,
    Typedef,
    Variable,
    Field,
    Specialization
};

MRDOCS_DECL dom::String toString(InfoKind kind) noexcept;

/** Common properties of all symbols
*/
struct MRDOCS_VISIBLE
    Info
{
    /** The unique identifier for this symbol.
    */
    SymbolID id = SymbolID::zero;

    /** The unqualified name.
    */
    std::string Name;

    /** Kind of declaration.
    */
    InfoKind Kind;

    /** Declaration access.

        Class members use:
        @li `AccessKind::Public`,
        @li `AccessKind::Protected`, and
        @li `AccessKind::Private`.

        Namespace members use `AccessKind::None`.
    */
    AccessKind Access = AccessKind::None;

    /** Implicitly extracted flag.

        This flag distinguishes primary `Info` from `Info` dependencies.

        A primary `Info` is one which was extracted during AST traversal
        because it satisfied all configured conditions to be extracted.

        An `Info` dependency is one which does not meet the configured
        conditions for extraction, but was extracted due to it being used
        by a primary `Info`.
    */
    bool Implicit = true;

    /** In-order List of parent namespaces.
    */
    std::vector<SymbolID> Namespace;

    /** The extracted javadoc for this declaration.
    */
    std::unique_ptr<Javadoc> javadoc;

    //--------------------------------------------

    virtual ~Info() = default;
    Info(Info const& Other) = delete;
    Info(Info&& Other) = default;

    explicit
    Info(
        InfoKind kind,
        SymbolID ID = SymbolID::zero) noexcept
        : id(ID)
        , Kind(kind)
    {
    }

    //
    // Observers
    //

    MRDOCS_DECL
    std::string
    extractName() const;

    constexpr bool isNamespace()      const noexcept { return Kind == InfoKind::Namespace; }
    constexpr bool isRecord()         const noexcept { return Kind == InfoKind::Record; }
    constexpr bool isFunction()       const noexcept { return Kind == InfoKind::Function; }
    constexpr bool isEnum()           const noexcept { return Kind == InfoKind::Enum; }
    constexpr bool isTypedef()        const noexcept { return Kind == InfoKind::Typedef; }
    constexpr bool isVariable()       const noexcept { return Kind == InfoKind::Variable; }
    constexpr bool isField()          const noexcept { return Kind == InfoKind::Field; }
    constexpr bool isSpecialization() const noexcept { return Kind == InfoKind::Specialization; }
};

//------------------------------------------------

/** Base class for providing variant discriminator functions.

    This offers functions that return a boolean at
    compile-time, indicating if the most-derived
    class is a certain type.
*/
template<InfoKind K>
struct IsInfo : Info
{
    /** The variant discriminator constant of the most-derived class.
    */
    static constexpr InfoKind kind_id = K;

    static constexpr bool isNamespace()      noexcept { return K == InfoKind::Namespace; }
    static constexpr bool isRecord()         noexcept { return K == InfoKind::Record; }
    static constexpr bool isFunction()       noexcept { return K == InfoKind::Function; }
    static constexpr bool isEnum()           noexcept { return K == InfoKind::Enum; }
    static constexpr bool isTypedef()        noexcept { return K == InfoKind::Typedef; }
    static constexpr bool isVariable()       noexcept { return K == InfoKind::Variable; }
    static constexpr bool isField()          noexcept { return K == InfoKind::Field; }
    static constexpr bool isSpecialization() noexcept { return K == InfoKind::Specialization; }

protected:
    constexpr IsInfo()
        : Info(K)
    {
    }

    constexpr explicit IsInfo(SymbolID ID)
        : Info(K, ID)
    {
    }
};

/** Invoke a function object with an Info-derived type.
*/
template<
    class InfoTy,
    class Fn,
    class... Args>
    requires std::derived_from<InfoTy, Info>
decltype(auto)
visit(
    InfoTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Info>(
        info, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(info.Kind)
    {
    case InfoKind::Namespace:
        return visitor.template visit<NamespaceInfo>();
    case InfoKind::Record:
        return visitor.template visit<RecordInfo>();
    case InfoKind::Function:
        return visitor.template visit<FunctionInfo>();
    case InfoKind::Enum:
        return visitor.template visit<EnumInfo>();
    case InfoKind::Typedef:
        return visitor.template visit<TypedefInfo>();
    case InfoKind::Variable:
        return visitor.template visit<VariableInfo>();
    case InfoKind::Field:
        return visitor.template visit<FieldInfo>();
    case InfoKind::Specialization:
        return visitor.template visit<SpecializationInfo>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // mrdocs
} // clang

#endif
