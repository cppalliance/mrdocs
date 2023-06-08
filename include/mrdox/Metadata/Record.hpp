//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_RECORD_HPP
#define MRDOX_API_METADATA_RECORD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/BitField.hpp>
#include <mrdox/Metadata/Field.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Reference.hpp>
#include <mrdox/Metadata/Scope.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Var.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdox {

/** Access specifier.

    Public is set to zero since it is the most
    frequently occurring access, and it is
    elided by the bitstream encoder because it
    has an all-zero bit pattern. This improves
    compression in the bitstream.

    @note It is by design that there is no
    constant to represent "none."
*/
enum class Access
{
    Public = 0,
    Protected,
    Private
};

/** A reference to a symbol, and an access specifier.

    This is used in records to refer to nested
    elements with access control.
*/
struct MemberRef
{
    SymbolID id;
    Access access;

    constexpr MemberRef() = default;

    constexpr MemberRef(
        const SymbolID& id_,
        Access access_)
    : id(id_)
    , access(access_)
    {
    }
};

/** Bit constants used with Record metadata
*/
union RecFlags0
{
    BitFieldFullValue raw{.value=0u};

    BitFlag<0> isFinal;
    BitFlag<1> isFinalDestructor;
};

/** Metadata for a direct base.
*/
struct BaseInfo
{
    SymbolID id;
    std::string Name;
    Access access;
    bool IsVirtual;

    BaseInfo(
        SymbolID const& id_ = SymbolID::zero,
        std::string_view Name_ = "",
        Access access_ = Access::Public,
        bool IsVirtual_ = false)
        : id(id_)
        , Name(Name_)
        , access(access_)
        , IsVirtual(IsVirtual_)
    {
    }
};

/** Members of a class, struct, or union.
*/
struct RecordScope
{
    std::vector<MemberRef> Records;
    std::vector<MemberRef> Functions;
    std::vector<MemberRef> Enums;
    std::vector<MemberRef> Types;
    std::vector<MemberRef> Fields;
    std::vector<MemberRef> Vars;
    std::vector<SymbolID> Specializations;
};

enum class RecordKeyKind
{
    Struct,
    Class,
    Union,
    // KRYSTIAN NOTE: __interface is a Microsoft extension,
    // do we want to support it?
    Interface
};

/** Metadata for struct, class, or union.
*/
struct RecordInfo
    : SymbolInfo
{
    friend class ASTVisitor;

    /** Kind of record this is (class, struct, or union).
    */
    RecordKeyKind KeyKind = RecordKeyKind::Struct;

    // When present, this record is a template or specialization.
    std::unique_ptr<TemplateInfo> Template;

    // Indicates if the record was declared using a typedef. Things like anonymous
    // structs in a typedef:
    //   typedef struct { ... } foo_t;
    // are converted into records with the typedef as the Name + this flag set.
    // KRYSTIAN FIXME: this does not account for alias-declarations
    bool IsTypeDef = false;

    RecFlags0 specs;

    /** List of immediate bases.
    */
    std::vector<BaseInfo> Bases;

    /** List of friend functions.
    */
    std::vector<SymbolID> Friends;

    /** Record members
    */
    RecordScope Members;

    //--------------------------------------------

    static constexpr InfoKind kind_id = InfoKind::Record;

    explicit
    RecordInfo(
        SymbolID ID = SymbolID::zero)
        : SymbolInfo(InfoKind::Record, ID)
    {
    }

private:
    explicit
    RecordInfo(
        std::unique_ptr<TemplateInfo>&& T)
        : SymbolInfo(InfoKind::Record)
        , Template(std::move(T))
    {
    }
};

struct DataMember
{
    FieldInfo const* I;
    RecordInfo const* From;
};

struct MemberEnum
{
    EnumInfo const* I;
    RecordInfo const* From;
};

struct MemberFunction
{
    FunctionInfo const* I;
    RecordInfo const* From;
};

struct MemberRecord
{
    RecordInfo const* I;
    RecordInfo const* From;
};

struct MemberType
{
    TypedefInfo const* I;
    RecordInfo const* From;
};

struct StaticDataMember
{
    VarInfo const* I;
    RecordInfo const* From;
};

} // mrdox
} // clang

#endif
