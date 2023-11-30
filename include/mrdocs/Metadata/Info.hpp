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
struct FriendInfo;
struct EnumeratorInfo;
struct GuideInfo;

/** Info variant discriminator
*/
enum class InfoKind
{
    Namespace = 1, // for bitstream
    Record,
    Function,
    Enum,
    Typedef,
    Variable,
    Field,
    Specialization,
    Friend,
    Enumerator,
    Guide
};

MRDOCS_DECL dom::String toString(InfoKind kind) noexcept;

/** Common properties of all symbols
*/
struct MRDOCS_VISIBLE
    Info
{
    /** The unique identifier for this symbol.
    */
    SymbolID id;

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
    bool Implicit = false;

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
        SymbolID ID) noexcept
        : id(ID)
        , Kind(kind)
    {
    }

    constexpr bool isNamespace()      const noexcept { return Kind == InfoKind::Namespace; }
    constexpr bool isRecord()         const noexcept { return Kind == InfoKind::Record; }
    constexpr bool isFunction()       const noexcept { return Kind == InfoKind::Function; }
    constexpr bool isEnum()           const noexcept { return Kind == InfoKind::Enum; }
    constexpr bool isTypedef()        const noexcept { return Kind == InfoKind::Typedef; }
    constexpr bool isVariable()       const noexcept { return Kind == InfoKind::Variable; }
    constexpr bool isField()          const noexcept { return Kind == InfoKind::Field; }
    constexpr bool isSpecialization() const noexcept { return Kind == InfoKind::Specialization; }
    constexpr bool isFriend()         const noexcept { return Kind == InfoKind::Friend; }
    constexpr bool isEnumerator()     const noexcept { return Kind == InfoKind::Enumerator; }
    constexpr bool isGuide()          const noexcept { return Kind == InfoKind::Guide; }
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
    static constexpr bool isFriend()         noexcept { return K == InfoKind::Friend; }
    static constexpr bool isEnumerator()     noexcept { return K == InfoKind::Enumerator; }
    static constexpr bool isGuide()          noexcept { return K == InfoKind::Guide; }

protected:
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
    case InfoKind::Friend:
        return visitor.template visit<FriendInfo>();
    case InfoKind::Enumerator:
        return visitor.template visit<EnumeratorInfo>();
    case InfoKind::Guide:
        return visitor.template visit<GuideInfo>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // mrdocs
} // clang

#endif
