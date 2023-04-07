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

#ifndef MRDOX_JAD_RECORD_HPP
#define MRDOX_JAD_RECORD_HPP

#include "jad/AccessScope.hpp"
#include "jad/MemberType.hpp"
#include "jad/Reference.hpp"
#include "jad/Scope.hpp"
#include "jad/Symbol.hpp"
#include "jad/Template.hpp"
#include "jad/Types.hpp"
#include <clang/AST/Type.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <vector>

namespace clang {
namespace mrdox {

struct BaseRecordInfo;

// TODO: Expand to allow for documenting templating, inheritance access,
// friend classes
// Info for types.
struct RecordInfo : SymbolInfo
{
    RecordInfo(
        SymbolID USR = SymbolID(),
        llvm::StringRef Name = llvm::StringRef(),
        llvm::StringRef Path = llvm::StringRef());

    void merge(RecordInfo&& I);

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

    llvm::SmallVector<MemberTypeInfo, 4> Members;   // List of info about record members.
    llvm::SmallVector<Reference, 4> Parents;        // List of base/parent records
                                                    // (does not include virtual parents).
    llvm::SmallVector<Reference, 4> VirtualParents; // List of virtual base/parent records.

    std::vector<BaseRecordInfo> Bases;              // List of base/parent records; this includes inherited methods and attributes

    Scope Children;
    //AccessScope scope;
};

//------------------------------------------------

struct BaseRecordInfo : public RecordInfo
{
    BaseRecordInfo();

    BaseRecordInfo(
        SymbolID USR,
        llvm::StringRef Name,
        llvm::StringRef Path,
        bool IsVirtual,
        AccessSpecifier Access,
        bool IsParent);

    // Indicates if base corresponds to a virtual inheritance
    bool IsVirtual = false;

    // Access level associated with this inherited info (public, protected,
    // private).
    AccessSpecifier Access = AccessSpecifier::AS_public;

    bool IsParent = false; // Indicates if this base is a direct parent
};

} // mrdox
} // clang

#endif
