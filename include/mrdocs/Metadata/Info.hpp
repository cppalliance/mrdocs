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

/* Forward declarations
 */
#define INFO(Type) struct Type##Info;
#include <mrdocs/Metadata/InfoNodesPascal.inc>

/** Info variant discriminator
*/
enum class InfoKind
{
    None = 0,
    #define INFO(Type) Type,
    #include <mrdocs/Metadata/InfoNodesPascal.inc>
};

/** Return the name of the InfoKind as a string.
 */
MRDOCS_DECL
dom::String
toString(InfoKind kind) noexcept;

/** Return the InfoKind from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    InfoKind kind)
{
    v = toString(kind);
}

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

    /** The parent symbol, if any.

        This is the parent namespace or record
        where the symbol is defined.
     */
    SymbolID Parent;

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

    #define INFO(Type) constexpr bool is##Type() const noexcept { return Kind == InfoKind::Type; }
    #include <mrdocs/Metadata/InfoNodesPascal.inc>
};

//------------------------------------------------

/** Base class for providing variant discriminator functions.

    This offers functions that return a boolean at
    compile-time, indicating if the most-derived
    class is a certain type.
*/
template <InfoKind K>
struct InfoCommonBase : Info
{
    /** The variant discriminator constant of the most-derived class.

        It only distinguishes from `Info::kind` in that it is a constant.

     */
    static constexpr InfoKind kind_id = K;

    #define INFO(Kind) \
    static constexpr bool is##Kind() noexcept { return K == InfoKind::Kind; }
    #include <mrdocs/Metadata/InfoNodesPascal.inc>

protected:
    constexpr explicit InfoCommonBase(SymbolID ID)
        : Info(K, ID)
    {
    }
};

/** Invoke a function object with a type derived from Info

    This function will invoke the function object `fn` with
    a type derived from `Info` as the first argument, followed
    by `args...`. The type of the first argument is determined
    by the `InfoKind` of the `Info` object.

    @param info The Info object to visit
    @param fn The function object to call
    @param args Additional arguments to pass to the function object
    @return The result of calling the function object with the derived type
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
    #define INFO(Type) \
    case InfoKind::Type: \
        return visitor.template visit<Type##Info>();
    #include <mrdocs/Metadata/InfoNodesPascal.inc>
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
    { t.Members } -> std::ranges::range;
    requires std::convertible_to<std::ranges::range_value_t<decltype(t.Members)>, SymbolID const&>;
};

/** Return the Info to a @ref dom::Value object.
 */
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Info const& I,
    DomCorpus const* domCorpus);

} // mrdocs
} // clang

#endif
