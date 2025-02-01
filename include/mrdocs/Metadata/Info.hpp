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
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/ExtractionMode.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <string>

namespace clang::mrdocs {

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
    InfoKind const kind)
{
    v = toString(kind);
}

/** Base class with common properties of all symbols
*/
struct MRDOCS_DECL Info
    : SourceInfo
{
    /** The unique identifier for this symbol.
    */
    SymbolID id;

    /** The unqualified name.
    */
    std::string Name;

    /** Kind of declaration.
    */
    InfoKind Kind = InfoKind::None;

    /** Declaration access.

        Class members use:
        @li `AccessKind::Public`,
        @li `AccessKind::Protected`, and
        @li `AccessKind::Private`.

        Namespace members use `AccessKind::None`.
    */
    AccessKind Access = AccessKind::None;

    /** Determine why a symbol is extracted.

        This flag distinguishes `Info` from its dependencies and
        indicates why it was extracted.

        Non-dependencies can be extracted in normal mode,
        see-below mode, or implementation-defined mode.

        A dependency is a symbol which does not meet the configured
        conditions for extraction, but had to be extracted due to it
        being used transitively by a primary `Info`.
    */
    ExtractionMode Extraction = ExtractionMode::Dependency;

    /** The parent symbol, if any.

        This is the parent namespace or record
        where the symbol is defined.
     */
    SymbolID Parent;

    /** The extracted javadoc for this declaration.
     */
    std::optional<Javadoc> javadoc;

    //--------------------------------------------

    ~Info() override = default;

    Info(Info const& Other) = default;

    /** Move constructor.
     */
    Info(Info&& Other) = default;

    /** Construct an Info.

        @param kind The kind of symbol
        @param ID The unique identifier for this symbol
    */
    explicit
    Info(
        InfoKind const kind,
        SymbolID const& ID) noexcept
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
    constexpr explicit InfoCommonBase(SymbolID const& ID)
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

/** Merges two Info objects.

    This function is used to merge two Info objects with the same SymbolID.
    The function assumes that the two Info objects are of the same type.
    If they are not, the function will fail.

    @param I The Info object to merge into.
    @param Other The Info object to merge from.
*/
MRDOCS_DECL
void
merge(Info& I, Info&& Other);

/** Merges two Info objects according to the behavior of the derived class.
 */
template <polymorphic_storage_for<Info> InfoTy>
void
merge(InfoTy& I, InfoTy&& Other)
{
    MRDOCS_ASSERT(I.Kind == Other.Kind);
    MRDOCS_ASSERT(I.id == Other.id);
    Info& base = I;
    visit(base, [&]<typename DerivedInfoTy>(DerivedInfoTy& derived) mutable
    {
        DerivedInfoTy& otherDerived = static_cast<DerivedInfoTy&>(Other);
        merge(derived, std::move(otherDerived));
    });
}

inline
bool
canMerge(Info const& I, Info const& Other)
{
    return
        I.Kind == Other.Kind &&
        I.id == Other.id;
}

/** A concept for types that have `Info` members.

    In most cases `T` is another `Info` type that
    has a `Members` member which is a range of
    `SymbolID` values.

    However, an @ref OverloadSet is also a type that
    contains `Info` members without being `Info` itself.
*/
template <class InfoTy>
concept InfoParent = requires(InfoTy const& I)
{
    { allMembers(I) } -> range_of<SymbolID>;
};

/** Map the Info to a @ref dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Info const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    io.map("id", I.id);
    if (!I.Name.empty())
    {
        io.map("name", I.Name);
    }
    io.map("kind", I.Kind);
    io.map("access", I.Access);
    io.map("extraction", I.Extraction);
    io.map("isRegular", I.Extraction == ExtractionMode::Regular);
    io.map("isSeeBelow", I.Extraction == ExtractionMode::SeeBelow);
    io.map("isImplementationDefined", I.Extraction == ExtractionMode::ImplementationDefined);
    io.map("isDependency", I.Extraction == ExtractionMode::Dependency);
    if (I.Parent)
    {
        io.map("parent", I.Parent);
        io.defer("parents", [&]
        {
            return getParents(*domCorpus, I);
        });
    }
    if (I.javadoc)
    {
        io.map("doc", *I.javadoc);
    }
    io.map("loc", dynamic_cast<SourceInfo const&>(I));
}

/** Map the Polymorphic Info to a @ref dom::Object.
 */
template <class IO, polymorphic_storage_for<Info> PolymorphicInfo>
requires std::derived_from<PolymorphicInfo, Info>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    PolymorphicInfo const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::LazyObjectMapTag{},
            io,
            U,
            domCorpus);
    });
}

/** Return the Info as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Info const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Map the Polymorphic Info as a @ref dom::Value object.
 */
template <class IO, polymorphic_storage_for<Info> InfoTy>
requires std::derived_from<InfoTy, Info>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    InfoTy const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::ValueFromTag{},
            io,
            U,
            domCorpus);
    });
}

/** Compare two Info objects
 */
MRDOCS_DECL
bool
operator<(
    Info const& lhs,
    Info const& rhs) noexcept;

} // clang::mrdocs

#endif
