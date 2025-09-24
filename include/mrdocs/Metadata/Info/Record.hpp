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

#ifndef MRDOCS_API_METADATA_INFO_RECORD_HPP
#define MRDOCS_API_METADATA_INFO_RECORD_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info/Friend.hpp>
#include <mrdocs/Metadata/Info/RecordBase.hpp>
#include <mrdocs/Metadata/Info/RecordInterface.hpp>
#include <mrdocs/Metadata/Info/RecordKeyKind.hpp>
#include <mrdocs/Metadata/Template.hpp>

namespace clang::mrdocs {

/** Metadata for struct, class, or union.
*/
struct RecordInfo final
    : InfoCommonBase<InfoKind::Record>
{
    /** Kind of record this is (class, struct, or union).
    */
    RecordKeyKind KeyKind = RecordKeyKind::Struct;

    /// When present, this record is a template or specialization.
    std::optional<TemplateInfo> Template;

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

    explicit RecordInfo(SymbolID const& ID) noexcept
        : InfoCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(RecordInfo const& other) const;
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
allMembers(RecordInfo const& T)
{
    return allMembers(T.Interface);
}

MRDOCS_DECL
void
merge(RecordInfo& I, RecordInfo&& Other);

/** Map a RecordInfo to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The RecordInfo to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    RecordInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
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

/** Map the RecordInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_RECORD_HPP
