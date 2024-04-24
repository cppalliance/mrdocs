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

struct AliasInfo;
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
struct UsingInfo;

/** Info variant discriminator
*/
enum class InfoKind
{
    /// The symbol is a namespace
    Namespace = 1, // for bitstream
    /// The symbol is a record (class or struct)
    Record,
    /// The symbol is a function
    Function,
    /// The symbol is an enum
    Enum,
    /// The symbol is a typedef
    Typedef,
    /// The symbol is a variable
    Variable,
    /// The symbol is a field
    Field,
    /// The symbol is a template specialization
    Specialization,
    /// The symbol is a friend declaration
    Friend,
    /// The symbol is an enumerator
    Enumerator,
    /// The symbol is a deduction guide
    Guide,
    /// The symbol is a namespace alias
    Alias,
    /// The symbol is a using declaration and using directive
    Using,
};

/** Return the name of the InfoKind as a string.
 */
MRDOCS_DECL
dom::String
toString(InfoKind kind) noexcept;

/** Base class with common properties of all symbols
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

        This flag distinguishes primary `Info` from its dependencies.

        A primary `Info` is one which was extracted during AST traversal
        because it satisfied all configured conditions to be extracted.

        An `Info` dependency is one which does not meet the configured
        conditions for extraction, but had to be extracted due to it
        being used transitively by a primary `Info`.
    */
    bool Implicit = false;

    /** Ordered list of parent namespaces.
     */
    std::vector<SymbolID> Namespace;

    /** The extracted javadoc for this declaration.
     */
    std::unique_ptr<Javadoc> javadoc;

    //--------------------------------------------

    virtual ~Info() = default;
    Info(Info const& Other) = delete;

    /** Move constructor.
     */
    Info(Info&& Other) = default;

    /** Construct an Info.

        @param kind The kind of symbol
        @param ID The unique identifier for this symbol
    */
    explicit
    Info(
        InfoKind kind,
        SymbolID ID) noexcept
        : id(ID)
        , Kind(kind)
    {
    }

    /// Determine if this symbol is a namespace.
    constexpr bool isNamespace()      const noexcept { return Kind == InfoKind::Namespace; }

    /// Determine if this symbol is a record (class or struct).
    constexpr bool isRecord()         const noexcept { return Kind == InfoKind::Record; }

    /// Determine if this symbol is a function.
    constexpr bool isFunction()       const noexcept { return Kind == InfoKind::Function; }

    /// Determine if this symbol is an enum.
    constexpr bool isEnum()           const noexcept { return Kind == InfoKind::Enum; }

    /// Determine if this symbol is a typedef.
    constexpr bool isTypedef()        const noexcept { return Kind == InfoKind::Typedef; }

    /// Determine if this symbol is a variable.
    constexpr bool isVariable()       const noexcept { return Kind == InfoKind::Variable; }

    /// Determine if this symbol is a field.
    constexpr bool isField()          const noexcept { return Kind == InfoKind::Field; }

    /// Determine if this symbol is a template specialization.
    constexpr bool isSpecialization() const noexcept { return Kind == InfoKind::Specialization; }

    /// Determine if this symbol is a friend declaration.
    constexpr bool isFriend()         const noexcept { return Kind == InfoKind::Friend; }

    /// Determine if this symbol is an enumerator.
    constexpr bool isEnumerator()     const noexcept { return Kind == InfoKind::Enumerator; }

    /// Determine if this symbol is a deduction guide.
    constexpr bool isGuide()          const noexcept { return Kind == InfoKind::Guide; }

    /// Determine if this symbol is a namespace alias.
    constexpr bool isAlias()          const noexcept { return Kind == InfoKind::Alias; }

    /// Determine if this symbol is a using declaration or using directive.
    constexpr bool isUsing()          const noexcept { return Kind == InfoKind::Using; }
};

//------------------------------------------------

/** Base class for providing variant discriminator functions.

    This offers functions that return a boolean at
    compile-time, indicating if the most-derived
    class is a certain type.
*/
template <InfoKind K>
struct IsInfo : Info
{
    /** The variant discriminator constant of the most-derived class.

        It only distinguishes from `Info::kind` in that it is a constant.

     */
    static constexpr InfoKind kind_id = K;

    /// Determine if this symbol is a namespace.
    static constexpr bool isNamespace()      noexcept { return K == InfoKind::Namespace; }

    /// Determine if this symbol is a record (class or struct).
    static constexpr bool isRecord()         noexcept { return K == InfoKind::Record; }

    /// Determine if this symbol is a function.
    static constexpr bool isFunction()       noexcept { return K == InfoKind::Function; }

    /// Determine if this symbol is an enum.
    static constexpr bool isEnum()           noexcept { return K == InfoKind::Enum; }

    /// Determine if this symbol is a typedef.
    static constexpr bool isTypedef()        noexcept { return K == InfoKind::Typedef; }

    /// Determine if this symbol is a variable.
    static constexpr bool isVariable()       noexcept { return K == InfoKind::Variable; }

    /// Determine if this symbol is a field.
    static constexpr bool isField()          noexcept { return K == InfoKind::Field; }

    /// Determine if this symbol is a template specialization.
    static constexpr bool isSpecialization() noexcept { return K == InfoKind::Specialization; }

    /// Determine if this symbol is a friend declaration.
    static constexpr bool isFriend()         noexcept { return K == InfoKind::Friend; }

    /// Determine if this symbol is an enumerator.
    static constexpr bool isEnumerator()     noexcept { return K == InfoKind::Enumerator; }

    /// Determine if this symbol is a deduction guide.
    static constexpr bool isGuide()          noexcept { return K == InfoKind::Guide; }

    /// Determine if this symbol is a namespace alias.
    static constexpr bool isAlias()          noexcept { return K == InfoKind::Alias; }

    /// Determine if this symbol is a using declaration or using directive.
    static constexpr bool isUsing()          noexcept { return K == InfoKind::Using; }

protected:
    constexpr explicit IsInfo(SymbolID ID)
        : Info(K, ID)
    {
    }
};

/** Invoke a function object with a type derived from Info

    This function will invoke the function object `fn` with
    a type derived from `Info` as the first argument, followed
    by `args...`. The type of the first argument is determined
    by the `InfoKind` of the `Info` object.

*/
template<
    std::derived_from<Info> InfoTy,
    class Fn,
    class... Args>
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
    case InfoKind::Alias:
        return visitor.template visit<AliasInfo>();
    case InfoKind::Using:
        return visitor.template visit<UsingInfo>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** A concept for types that have `Info` members.

    In most cases `T` is another `Info` type that
    has a `Members` member which is a range of
    `SymbolID` values.

    However, an @ref OverloadSet is also a type that
    contains `Info` members without being `Info` itself.
*/
template <class T>
concept InfoParent = requires(T const& t) {
    t.Members;
    requires std::ranges::range<decltype(std::declval<T>().Members)>;
    requires std::same_as<
        std::ranges::range_value_t<std::decay_t<decltype(std::declval<T>().Members)>>, SymbolID>;
};

} // mrdocs
} // clang

#endif
