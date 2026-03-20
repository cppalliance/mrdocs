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

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORD_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORD_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Metadata/Symbol/Friend.hpp>
#include <mrdocs/Metadata/Symbol/RecordBase.hpp>
#include <mrdocs/Metadata/Symbol/RecordInterface.hpp>
#include <mrdocs/Metadata/Symbol/RecordKeyKind.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Metadata for struct, class, or union.
*/
struct RecordSymbol final
    : SymbolCommonBase<SymbolKind::Record>
{
    /** Kind of record this is (class, struct, or union).
    */
    RecordKeyKind KeyKind = RecordKeyKind::Struct;

    /// When present, this record is a template or specialization.
    Optional<TemplateInfo> Template;

    /** Whether the record originated from a typedef-style declaration.

        Things like anonymous structs in a typedef:

        @code
        typedef struct { ... } foo_t;
        @endcode

        are converted into records with the typedef as the Name + this flag set.

        @note Alias-declarations are not yet distinguished here.
    */
    bool IsTypeDef = false;

    /** Whether the class is marked `final`.
    */
    bool IsFinal = false;

    /** Whether the destructor is marked `final`.
    */
    bool IsFinalDestructor = false;

    /** List of immediate bases.
    */
    std::vector<BaseInfo> Bases;

    /** List of derived classes
    */
    std::vector<SymbolID> Derived;

    /** Lists of members.
    */
    RecordInterface Interface;

    /** List of friends.
    */
    std::vector<FriendInfo> Friends;

    //--------------------------------------------

    /** Create a record symbol bound to an ID.
    */
    explicit RecordSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare records including bases, members, and flags.
    */
    std::strong_ordering
    operator<=>(RecordSymbol const& other) const;
};

MRDOCS_DESCRIBE_STRUCT(
    RecordSymbol,
    (Symbol),
    (KeyKind, Template, IsTypeDef, IsFinal, IsFinalDestructor,
     Bases, Derived, Interface, Friends)
)

/** Return the default accessibility for a record key kind.
*/
constexpr
std::string_view
getDefaultAccessString(
    RecordKeyKind const& kind) noexcept
{
    switch(kind)
    {
    case RecordKeyKind::Class:
        return "private";
    case RecordKeyKind::Struct:
    case RecordKeyKind::Union:
        return "public";
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** View all record members across access levels.
    @return Lazy view traversing every tranche.
*/
inline
auto
allMembers(RecordSymbol const& T)
{
    return allMembers(T.Interface);
}

/** Merge metadata from another record of the same identity.
*/
MRDOCS_DECL
void
merge(RecordSymbol& I, RecordSymbol&& Other);

/** Map a RecordSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The RecordSymbol to map.
    @param domCorpus The DomCorpus used to create
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    RecordSymbol const& I,
    DomCorpus const* domCorpus);

/** Map the RecordSymbol to a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORD_HPP
