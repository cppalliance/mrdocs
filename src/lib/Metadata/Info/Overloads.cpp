//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Radix.hpp"
#include <format>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Info/Function.hpp>
#include <mrdocs/Metadata/Info/Namespace.hpp>
#include <mrdocs/Metadata/Info/Overloads.hpp>

namespace clang::mrdocs {

OverloadsInfo::OverloadsInfo(SymbolID const &Parent, std::string_view Name,
                             AccessKind access, bool isStatic) noexcept
    : InfoCommonBase(SymbolID::createFromString(std::format(
          "{}-{}-{}-{}", toBase16(Parent), Name, toString(access), isStatic))) {
  this->Parent = Parent;
  if (isStatic) this->StorageClass = StorageClassKind::Static;
}

void
merge(OverloadsInfo& I, OverloadsInfo&& Other)
{
    merge(I.asInfo(), std::move(Other.asInfo()));
    namespace stdr = std::ranges;
    namespace stdv = std::ranges::views;
    auto newMembers = stdv::filter(Other.Members, [&](auto const& Member) {
        return stdr::find(I.Members, Member) == I.Members.end();
    });
    I.Members.insert(I.Members.end(), newMembers.begin(), newMembers.end());
}

void
addMember(OverloadsInfo& I, FunctionInfo const& Member)
{
    if (I.Members.empty())
    {
        I.Name = Member.Name;
        I.Access = Member.Access;
        I.Extraction = Member.Extraction;
        I.Class = Member.Class;
        I.OverloadedOperator = Member.OverloadedOperator;
        I.ReturnType = Member.ReturnType;
    }
    else
    {
        I.Extraction = leastSpecific(I.Extraction, Member.Extraction);
        if (I.ReturnType != Member.ReturnType)
        {
            I.ReturnType = std::nullopt;
        }
    }
    merge(I.asSourceInfo(), Member.asSourceInfo());
    I.Members.push_back(Member.id);
}

} // clang::mrdocs
