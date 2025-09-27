//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Radix.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/Overloads.hpp>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include <format>

namespace mrdocs {

OverloadsSymbol::OverloadsSymbol(SymbolID const &Parent, std::string_view Name,
                             AccessKind access, bool isStatic) noexcept
    : SymbolCommonBase(SymbolID::createFromString(std::format(
          "{}-{}-{}-{}", toBase16(Parent), Name, toString(access), isStatic))) {
  this->Parent = Parent;
}

void
merge(OverloadsSymbol& I, OverloadsSymbol&& Other)
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
addMember(OverloadsSymbol& I, FunctionSymbol const& Member)
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
            // The return types differ, so we use 'auto' to indicate that.
            I.ReturnType = Polymorphic<Type>(AutoType{});
        }
    }
    merge(I.Loc, Member.Loc);
    I.Members.push_back(Member.id);
}

} // mrdocs
