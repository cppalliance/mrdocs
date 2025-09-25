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

#ifndef MRDOCS_API_METADATA_INFO_INFOBASE_HPP
#define MRDOCS_API_METADATA_INFO_INFOBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info/ExtractionMode.hpp>
#include <mrdocs/Metadata/Info/InfoKind.hpp>
#include <mrdocs/Metadata/Info/Source.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>

namespace clang::mrdocs {

/* Forward declarations
 */
#define INFO(Type) struct Type##Info;
#include <mrdocs/Metadata/Info/InfoNodes.inc>

/** Base class with common properties of all symbols
*/
struct MRDOCS_VISIBLE Info
{
    /** The source location information.
     */
    SourceInfo Loc;

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

        A dependency is a symbol that does not meet the configured
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
    Optional<Javadoc> javadoc;

    //--------------------------------------------

    virtual ~Info() = default;

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == InfoKind::Type; \
    }
#include <mrdocs/Metadata/Info/InfoNodes.inc>

    constexpr Info const& asInfo() const noexcept
    {
        return *this;
    }

    constexpr Info& asInfo() noexcept
    {
        return *this;
    }

    #define INFO(Type) \
    constexpr Type##Info const& as##Type() const noexcept { \
        if (Kind == InfoKind::Type) \
            return reinterpret_cast<Type##Info const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Info/InfoNodes.inc>

#define INFO(Type) \
    constexpr Type##Info & as##Type() noexcept { \
        if (Kind == InfoKind::Type) \
            return reinterpret_cast<Type##Info&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Info/InfoNodes.inc>

#define INFO(Type) \
    constexpr Type##Info const* as##Type##Ptr() const noexcept { \
        if (Kind == InfoKind::Type) { return reinterpret_cast<Type##Info const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Info/InfoNodes.inc>

#define INFO(Type) \
    constexpr Type##Info * as##Type##Ptr() noexcept { \
        if (Kind == InfoKind::Type) { return reinterpret_cast<Type##Info *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Info/InfoNodes.inc>

    auto operator<=>(Info const&) const = default;

protected:
    constexpr Info() = default;

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
#include <mrdocs/Metadata/Info/InfoNodes.inc>

    auto operator<=>(InfoCommonBase const&) const = default;

protected:
    InfoCommonBase() = default;

    constexpr explicit InfoCommonBase(SymbolID const& ID)
        : Info(K, ID)
    {
    }
};

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

inline
bool
canMerge(Info const& I, Info const& Other)
{
    return
        I.Kind == Other.Kind &&
        I.id == Other.id;
}

/** Map the Info to a @ref dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The Info to convert.
    @param domCorpus The DomCorpus used to resolve references.
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
    io.map("class", std::string("symbol"));
    io.map("kind", I.Kind);
    io.map("id", I.id);
    if (!I.Name.empty())
    {
        io.map("name", I.Name);
    }
    io.map("access", I.Access);
    io.map("extraction", I.Extraction);
    io.map("isRegular", I.Extraction == ExtractionMode::Regular);
    io.map("isSeeBelow", I.Extraction == ExtractionMode::SeeBelow);
    io.map("isImplementationDefined", I.Extraction == ExtractionMode::ImplementationDefined);
    io.map("isDependency", I.Extraction == ExtractionMode::Dependency);
    if (I.Parent)
    {
        io.map("parent", I.Parent);
    }
    if (I.javadoc)
    {
        io.map("doc", *I.javadoc);
    }
    io.map("loc", I.Loc);
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

inline
Optional<Location>
getPrimaryLocation(Info const& I)
{
    return getPrimaryLocation(
        I.Loc,
        I.isRecord() || I.isEnum());
}

} // clang::mrdocs

#endif
