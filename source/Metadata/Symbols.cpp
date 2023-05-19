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

#include <mrdox/Metadata/Symbols.hpp>
#include <climits>

namespace clang {
namespace mrdox {

// Better have 8 bits per byte, otherwise
// we are going to be having some problems...
static_assert(CHAR_BIT == 8);

constexpr
unsigned char
tolower(char c) noexcept
{
    auto uc = static_cast<unsigned char>(c);
    if(uc >= 'A' && uc <= 'Z')
        return uc + 32;
    return uc;
}

std::strong_ordering
compareSymbolNames(
    llvm::StringRef s0,
    llvm::StringRef s1) noexcept
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

} // mrdox
} // clang
