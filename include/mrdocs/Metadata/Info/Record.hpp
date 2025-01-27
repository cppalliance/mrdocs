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

#ifndef MRDOCS_API_METADATA_RECORD_HPP
#define MRDOCS_API_METADATA_RECORD_HPP

#include <mrdocs/ADT/PolymorphicValue.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Scope.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <vector>
#include <string_view>

namespace clang::mrdocs {

/** Metadata for a direct base.
*/
struct BaseInfo
{
    PolymorphicValue<TypeInfo> Type;
    AccessKind Access = AccessKind::Public;
    bool IsVirtual = false;

    BaseInfo() = default;

    BaseInfo(
        PolymorphicValue<TypeInfo>&& type,
        AccessKind const access,
        bool const is_virtual)
        : Type(std::move(type))
        , Access(access)
        , IsVirtual(is_virtual)
    {
    }
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BaseInfo const& I,
    DomCorpus const* domCorpus);

enum class RecordKeyKind
{
    Struct,
    Class,
    Union
};

MRDOCS_DECL dom::String toString(RecordKeyKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordKeyKind kind)
{
    v = toString(kind);
}

/** Metadata for struct, class, or union.
*/
struct RecordInfo final
    : InfoCommonBase<InfoKind::Record>
    , ScopeInfo
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

    //--------------------------------------------

    explicit RecordInfo(SymbolID const& ID) noexcept
        : InfoCommonBase(ID)
    {
    }
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

} // clang::mrdocs

#endif
