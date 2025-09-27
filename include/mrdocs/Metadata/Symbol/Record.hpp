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

    // Indicates if the record was declared using a typedef.
    // Things like anonymous structs in a typedef:
    //   typedef struct { ... } foo_t;
    // are converted into records with the typedef as the Name + this flag set.
    // KRYSTIAN FIXME: this does not account for alias-declarations
    bool IsTypeDef = false;

    bool IsFinal = false;
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

    explicit RecordSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(RecordSymbol const& other) const;
};

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

inline
auto
allMembers(RecordSymbol const& T)
{
    return allMembers(T.Interface);
}

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
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("tag", I.KeyKind);
    io.map("defaultAccess", getDefaultAccessString(I.KeyKind));
    io.map("isFinal", I.IsFinal);
    io.map("isTypedef", I.IsTypeDef);
    io.map("bases", dom::LazyArray(I.Bases, domCorpus));
    io.map("derived", dom::LazyArray(I.Derived, domCorpus));
    io.map("interface", I.Interface);
    io.map("template", I.Template);
    io.map("friends", dom::LazyArray(I.Friends, domCorpus));
}

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
