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

#ifndef MRDOX_METADATA_RECORD_HPP
#define MRDOX_METADATA_RECORD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Bits.hpp>
#include <mrdox/Metadata/MemberType.hpp>
#include <mrdox/Metadata/Reference.hpp>
#include <mrdox/Metadata/Scope.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <vector>

namespace clang {
namespace mrdox {

struct BaseRecordInfo;

/** Bit constants used with Record metadata
*/
enum class RecFlags0 : std::uint32_t
{
    isFinal             = 0x00000001,
    isFinalDestructor   = 0x00000002
};

struct RecordInfo : SymbolInfo
{
    // VFALCO Use our own enumeration for this
    // Type of this record (struct, class, union, interface).
    TagTypeKind TagType = TagTypeKind::TTK_Struct;

    // Full qualified name of this record, including namespaces and template
    // specializations.
    llvm::SmallString<16> FullName;

    // When present, this record is a template or specialization.
    llvm::Optional<TemplateInfo> Template;

    // Indicates if the record was declared using a typedef. Things like anonymous
    // structs in a typedef:
    //   typedef struct { ... } foo_t;
    // are converted into records with the typedef as the Name + this flag set.
    bool IsTypeDef = false;

    Bits<RecFlags0> specs;

    llvm::SmallVector<MemberTypeInfo, 4> Members;   // List of info about record members.
    llvm::SmallVector<Reference, 4> Parents;        // List of base/parent records
                                                    // (does not include virtual parents).
    llvm::SmallVector<Reference, 4> VirtualParents; // List of virtual base/parent records.

    std::vector<BaseRecordInfo> Bases;              // List of base/parent records; this includes inherited methods and attributes

    /** List of friend functions.
    */
    llvm::SmallVector<SymbolID, 4> Friends;

    Scope Children;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_record;

    MRDOX_DECL
    explicit
    RecordInfo(
        SymbolID id = SymbolID(),
        llvm::StringRef Name = llvm::StringRef());
};

} // mrdox
} // clang

#endif
