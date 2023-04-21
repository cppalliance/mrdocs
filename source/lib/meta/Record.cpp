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

#include "Reduce.hpp"
#include <mrdox/meta/Record.hpp>
#include <cassert>
#include <utility>

namespace clang {
namespace mrdox {

RecordInfo::
RecordInfo(
    SymbolID USR,
    StringRef Name,
    StringRef Path)
    : SymbolInfo(InfoType::IT_record, USR, Name, Path)
    , Children(false)
{
}

void
RecordInfo::
merge(RecordInfo&& Other)
{
    assert(canMerge(Other));
    if (!TagType)
        TagType = Other.TagType;
    IsTypeDef = IsTypeDef || Other.IsTypeDef;
    if (Members.empty())
        Members = std::move(Other.Members);
    if (Bases.empty())
        Bases = std::move(Other.Bases);
    if (Parents.empty())
        Parents = std::move(Other.Parents);
    if (VirtualParents.empty())
        VirtualParents = std::move(Other.VirtualParents);
    // Reduce children if necessary.
    reduceChildren(Children.Records, std::move(Other.Children.Records));
    reduceChildren(Children.Functions, std::move(Other.Children.Functions));
    reduceChildren(Children.Enums, std::move(Other.Children.Enums));
    reduceChildren(Children.Typedefs, std::move(Other.Children.Typedefs));
    SymbolInfo::merge(std::move(Other));
    if (!Template)
        Template = Other.Template;
}

BaseRecordInfo::
BaseRecordInfo()
    : RecordInfo()
{
}

BaseRecordInfo::
BaseRecordInfo(
    SymbolID USR,
    StringRef Name,
    StringRef Path,
    bool IsVirtual,
    AccessSpecifier Access,
    bool IsParent)
    : RecordInfo(USR, Name, Path)
    , IsVirtual(IsVirtual)
    , Access(Access)
    , IsParent(IsParent)
{
}

} // mrdox
} // clang
