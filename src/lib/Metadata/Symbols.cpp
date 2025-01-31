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

#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <lib/Support/Radix.hpp>
#include <climits>
#include <ranges>
#include <llvm/Support/SHA1.h>

namespace clang {
namespace mrdocs {

// Better have 8 bits per byte, otherwise
// we are going to be having some problems...
static_assert(CHAR_BIT == 8);

SymbolID
SymbolID::
createFromString(std::string_view const input)
{
    // Compute the SHA1 hash of the input string
    llvm::SHA1 sha1;
    sha1.update(input);
    auto const result = sha1.final();
    static_assert(result.size() == 20);
    SymbolID const id(reinterpret_cast<SymbolID::value_type const *>(result.data()));
    return id;
}


constexpr
unsigned char
tolower(char c) noexcept
{
    auto uc = static_cast<unsigned char>(c);
    if(uc >= 'A' && uc <= 'Z')
        return uc + 32;
    return uc;
}

std::string
toBase16Str(SymbolID const& id)
{
    return toBase16(id);
}


std::strong_ordering
compareSymbolNames(
    std::string_view s0,
    std::string_view s1) noexcept
{
    auto i0 = s0.begin();
    auto i1 = s1.begin();
    int s_cmp = 0;
    while (i0 != s0.end() && i1 != s1.end())
    {
        char c0 = *i0;
        char c1 = *i1;
        auto lc0 = tolower(c0);
        auto lc1 = tolower(c1);
        if(lc0 < lc1)
            return std::strong_ordering::less;
        else if(lc0 > lc1)
            return std::strong_ordering::greater;
        if(c0 != c1)
        {
            s_cmp = c0 > c1 ? -1 : 1;
            goto do_tiebreak;
        }
        i0++, i1++;
    }
    if(s0.size() < s1.size())
        return std::strong_ordering::less;
    if(s0.size() > s1.size())
        return std::strong_ordering::greater;
    return std::strong_ordering::equivalent;
    //---
    while (i0 != s0.end() && i1 != s1.end())
    {
        {
            char c0 = *i0;
            char c1 = *i1;
            auto lc0 = tolower(c0);
            auto lc1 = tolower(c1);
            if(lc0 < lc1)
                return std::strong_ordering::less;
            else if(lc0 > lc1)
                return std::strong_ordering::greater;
        }
do_tiebreak:
        i0++, i1++;
    }
    if(s0.size() < s1.size())
        return std::strong_ordering::less;
    if(s0.size() > s1.size())
        return std::strong_ordering::greater;
    if(s_cmp < 0)
        return std::strong_ordering::less;
    if(s_cmp > 0)
        return std::strong_ordering::greater;
    return std::strong_ordering::equivalent;
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SymbolID const& id)
{
    if (id != SymbolID::invalid)
    {
        v = toBase16(id);
    }
    else
    {
        v = {};
    }
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SymbolID const& id,
    DomCorpus const* domCorpus)
{
    v = domCorpus->get(id);
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::unique_ptr<SymbolID> const& t,
    DomCorpus const* domCorpus)
{
    if (!t)
    {
        v = nullptr;
        return;
    }
    v = dom::ValueFrom(*t, domCorpus);
}


} // mrdocs
} // clang
