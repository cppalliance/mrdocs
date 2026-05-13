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

#ifndef MRDOCS_API_METADATA_SYMBOL_SYMBOLBASE_HPP
#define MRDOCS_API_METADATA_SYMBOL_SYMBOLBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Attributes.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>
#include <mrdocs/Metadata/Symbol/ExtractionMode.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Metadata/Symbol/SymbolKind.hpp>
#include <mrdocs/Support/CompareReflectedType.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>

namespace mrdocs {

/* Forward declarations
 */
#define INFO(Type) struct Type##Symbol;
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

/** Base class with common properties of all symbols
*/
struct MRDOCS_VISIBLE Symbol {
    /** The unqualified name.
    */
    std::string Name;

    /** The source location information.
    */
    SourceInfo Loc;

    /** Kind of declaration.
    */
    SymbolKind Kind = SymbolKind::None;

    /** The unique identifier for this symbol.
    */
    SymbolID id;

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

    /** Whether this a copy of an inherited method, as produced when
        `inherit-base-members` is not `never`.
    */
    bool IsCopyFromInherited = false;

    /** The parent symbol, if any.

        This is the parent namespace or record
        where the symbol is defined.
    */
    SymbolID Parent;

    /** The extracted documentation for this declaration.
    */
    Optional<DocComment> doc;

    /** The C++ attributes attached to this declaration.

        These are the attributes as written in the source,
        e.g. `[[deprecated]]` or `[[nodiscard]]`, captured for
        every symbol kind. See @ref Attribute for the per-attribute
        name and arguments.
    */
    std::vector<Polymorphic<Attribute>> Attributes;

    //--------------------------------------------

    /** Polymorphic base needs a virtual destructor.
    */
    virtual ~Symbol() = default;

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == SymbolKind::Type; \
    }
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

    /** View this instance as a const Symbol reference.
    */
    constexpr Symbol const& asInfo() const noexcept
    {
        return *this;
    }

    /** View this instance as a mutable Symbol reference.
    */
    constexpr Symbol& asInfo() noexcept
    {
        return *this;
    }

    #define INFO(Type) \
    constexpr Type##Symbol const& as##Type() const noexcept { \
        if (Kind == SymbolKind::Type) \
            return reinterpret_cast<Type##Symbol const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

#define INFO(Type) \
    constexpr Type##Symbol & as##Type() noexcept { \
        if (Kind == SymbolKind::Type) \
            return reinterpret_cast<Type##Symbol&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

#define INFO(Type) \
    constexpr Type##Symbol const* as##Type##Ptr() const noexcept { \
        if (Kind == SymbolKind::Type) { return reinterpret_cast<Type##Symbol const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

#define INFO(Type) \
    constexpr Type##Symbol * as##Type##Ptr() noexcept { \
        if (Kind == SymbolKind::Type) { return reinterpret_cast<Type##Symbol *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

protected:
    /** Default constructor for derived types.
    */
    constexpr Symbol() = default;

    /** Copy constructor.
    */
    Symbol(Symbol const& Other) = default;

    /** Move constructor.
    */
    Symbol(Symbol&& Other) = default;

    /** Construct a Symbol.

        @param kind The kind of symbol
        @param ID The unique identifier for this symbol
    */
    explicit
        Symbol(
            SymbolKind const kind,
            SymbolID const& ID) noexcept
        : Kind(kind)
        , id(ID)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    Symbol,
    (),
    (Name, Loc, Kind, id, Access,
     Extraction, IsCopyFromInherited, Parent, doc, Attributes)
)

/** Map a Symbol to a dom::Object plus a `class` discriminator.

    The `class` field is the only synthesized member here: templates
    that consume a mixed array of DOM objects use it to tell Symbol
    entries apart from Type or Name entries. Everything else comes
    from reflection over the described struct, including the
    `extraction` enum that templates compare with `(eq extraction
    "regular")` etc.

    @param io The IO object to map into.
    @param I The Symbol to map.
    @param domCorpus The DomCorpus context.
*/
template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Symbol const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    mapReflectedType<false>(io, I, domCorpus);
    io.map("class", std::string("symbol"));
}

//------------------------------------------------

/** Base class for providing variant discriminator functions.

    This offers functions that return a boolean at
    compile-time, indicating if the most-derived
    class is a certain type.
*/
template <SymbolKind K>
struct SymbolCommonBase : Symbol
{
    /** The variant discriminator constant of the most-derived class.

        It only distinguishes from `Symbol::kind` in that it is a constant.

    */
    static constexpr SymbolKind kind_id = K;

    #define INFO(Kind) \
    static constexpr bool is##Kind() noexcept { return K == SymbolKind::Kind; }
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

    MRDOCS_DESCRIBE_CLASS(SymbolCommonBase, (Symbol), ())

protected:
    /** Default constructor.
    */
    SymbolCommonBase() = default;

    /** Construct bound to an ID.
    */
    constexpr explicit SymbolCommonBase(SymbolID const& ID)
        : Symbol(K, ID)
    {
    }
};

/** Check whether two symbols may be merged.
    @return True when kinds and IDs match.
*/
inline
bool
canMerge(Symbol const& I, Symbol const& Other)
{
    return
        I.Kind == Other.Kind &&
        I.id == Other.id;
}

/** Determine a location to use when none is explicitly chosen.
*/
inline
Optional<Location>
getPrimaryLocation(Symbol const& I)
{
    return getPrimaryLocation(
        I.Loc,
        I.isRecord() || I.isEnum());
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_SYMBOLBASE_HPP
