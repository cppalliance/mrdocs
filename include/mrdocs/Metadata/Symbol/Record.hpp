//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
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
#include <mrdocs/Support/Reflection/Describe.hpp>

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

    /** Whether this record is listed on its primary's page.

        A presentation-layer flag set by `SpecializationFinalizer`
        when *both* conditions hold:

          1. This record is a template specialization (AST-local,
             equivalent to `Template->specializationKind() != Primary`).
          2. Its primary is being extracted in
             @ref ExtractionMode::Regular (cross-symbol, needs the
             corpus).

        When set, the record is rendered in its primary's
        "Specializations" section and suppressed from the parent
        scope's listing. Orphan specializations (primary excluded
        from extraction) fail condition 2 and keep the flag `false`,
        so they remain reachable from the parent scope. The name
        deliberately encodes the resulting placement rather than
        the AST property in 1, which `Template` already exposes.
    */
    bool IsListedOnPrimary = false;

    /** Specializations whose primary is this record.

        Populated by `SpecializationFinalizer` with the IDs of
        class-template specializations referring to this record
        as their primary. Sorted by referent name then ID.
    */
    std::vector<SymbolID> Specializations;

    /** Deduction guides associated with this class template.

        Populated by `SpecializationFinalizer` with the IDs of
        deduction guides that deduce this record. Sorted by
        referent name then ID.
    */
    std::vector<SymbolID> DeductionGuides;

    //--------------------------------------------

    /** Create a record symbol bound to an ID.
    */
    explicit RecordSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare records including bases, members, and flags.
    */
    MRDOCS_DECL
    std::strong_ordering
    operator<=>(RecordSymbol const& other) const;
};

MRDOCS_DESCRIBE_STRUCT(
    RecordSymbol,
    (SymbolCommonBase<SymbolKind::Record>),
    (KeyKind, Template, IsTypeDef, IsFinal, IsFinalDestructor,
     Bases, Derived, Interface, Friends,
     IsListedOnPrimary, Specializations, DeductionGuides)
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

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORD_HPP
