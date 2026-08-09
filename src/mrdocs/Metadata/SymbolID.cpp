//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Radix.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <llvm/Support/SHA1.h>
#include <array>
#include <climits>
#include <ranges>
#include <vector>


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
    return toBase16(std::string_view(
        reinterpret_cast<char const*>(id.data()), id.size()));
}

std::string
toBase58Str(SymbolID const& id)
{
    return toBase58(std::string_view(
        reinterpret_cast<char const*>(id.data()), id.size()));
}

std::optional<SymbolID>
fromBase58Str(std::string_view str)
{
    static constexpr char alphabet[] =
        "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

    // Reverse lookup: base58 value per ASCII code, -1 if not a digit.
    static auto const inverse = []
    {
        std::array<signed char, 256> table{};
        table.fill(-1);
        for (signed char i = 0; alphabet[i] != '\0'; ++i)
        {
            table[static_cast<unsigned char>(alphabet[i])] = i;
        }
        return table;
    }();

    constexpr std::size_t kIdBytes = 20;
    if (str.empty())
    {
        return std::nullopt;
    }

    // Leading '1's decode to leading zero bytes.
    std::size_t zeroes = 0;
    std::size_t i = 0;
    while (i < str.size() && str[i] == '1')
    {
        ++zeroes;
        ++i;
    }

    // Big-endian base256 accumulator (log(58)/log(256) ~ 0.733).
    std::vector<unsigned char> b256(str.size() * 733 / 1000 + 1, 0);
    std::size_t length = 0;
    for (; i < str.size(); ++i)
    {
        int carry = inverse[static_cast<unsigned char>(str[i])];
        if (carry < 0)
        {
            return std::nullopt;
        }
        std::size_t j = 0;
        for (auto it = b256.rbegin();
             (carry != 0 || j < length) && it != b256.rend();
             ++it, ++j)
        {
            carry += 58 * *it;
            *it = static_cast<unsigned char>(carry % 256);
            carry /= 256;
        }
        length = j;
    }

    // The value is `zeroes` leading zero bytes plus the last `length` bytes of
    // the accumulator; it must fit in the fixed-size SymbolID.
    if (zeroes + length > kIdBytes)
    {
        return std::nullopt;
    }
    SymbolID::value_type bytes[kIdBytes] = {};
    std::size_t out_pos = kIdBytes - length;
    for (std::size_t k = b256.size() - length; k < b256.size(); ++k, ++out_pos)
    {
        bytes[out_pos] = static_cast<SymbolID::value_type>(b256[k]);
    }
    return SymbolID(bytes);
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
        v = toBase58Str(id);
    }
    else
    {
        v = {};
    }
}


} // mrdocs

