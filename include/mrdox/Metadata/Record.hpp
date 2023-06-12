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
#include <mrdox/Metadata/Enum.hpp>
#include <mrdox/Metadata/Field.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Typedef.hpp>
#include <mrdox/Metadata/Var.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdox {

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
    AccessKind Access;
    bool IsVirtual;

    BaseInfo(
        SymbolID const& id_ = SymbolID::zero,
        std::string_view Name_ = "",
        AccessKind access = AccessKind::Public,
        bool IsVirtual_ = false)
        : id(id_)
        , Name(Name_)
        , Access(access)
        , IsVirtual(IsVirtual_)
    {
    }
};

enum class RecordKeyKind
{
    Struct,
    Class,
    Union
};

/** Metadata for struct, class, or union.
*/
struct RecordInfo
    : IsInfo<InfoKind::Record>
    , SymbolInfo
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
    std::vector<SymbolID> Members;

    /** Record member specializations
    */
    std::vector<SymbolID> Specializations;

    //--------------------------------------------

    explicit
    RecordInfo(
        SymbolID ID = SymbolID::zero)
        : IsInfo(ID)
    {
    }

private:
    explicit
    RecordInfo(
        std::unique_ptr<TemplateInfo>&& T)
        : Template(std::move(T))
    {
    }
};

} // mrdox
} // clang

#endif
