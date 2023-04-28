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
#include <mrdox/Metadata/BaseRecord.hpp>
#include <mrdox/Debug.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <llvm/ADT/STLExtras.h>
#include <cassert>
#include <utility>

namespace clang {
namespace mrdox {

RecordInfo::
RecordInfo(
    SymbolID USR,
    StringRef Name)
    : SymbolInfo(InfoType::IT_record, USR, Name)
    , Children(false)
{
}

void
RecordInfo::
merge(RecordInfo&& Other)
{
    Assert(canMerge(Other));
    if (!TagType)
        TagType = Other.TagType;
    IsTypeDef = IsTypeDef || Other.IsTypeDef;
    specs.merge(std::move(Other).specs);
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
    if(! Other.Friends.empty())
    {
        llvm::append_range(Friends, std::move(Other.Friends));
        llvm::sort(Friends,
            [](SymbolID const& id0, SymbolID const& id1)
            {
                return id0 < id1;
            });
        auto last = std::unique(Friends.begin(), Friends.end());
        Friends.erase(last, Friends.end());
    }
}

} // mrdox
} // clang
