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

#include "base64.hpp"

namespace clang {
namespace mrdox {

static
char const*
get_alphabet()
{
    static char constexpr tab[] = {
        "ABCDEFGHIJKLMNOP"
        "QRSTUVWXYZabcdef"
        "ghijklmnopqrstuv"
        "wxyz0123456789+/"
    };
    return &tab[0];
}

static
std::size_t constexpr
encoded_size(
    std::size_t n)
{
    return 4 * ((n + 2) / 3);
}

/** Encode a series of octets as a padded, base64 string.
    The resulting string will not be null terminated.
    @par Requires
    The memory pointed to by `out` points to valid memory
    of at least `encoded_size(len)` bytes.
    @return The number of characters written to `out`. This
    will exclude any null termination.
*/
static
std::size_t
encode(void* dest, void const* src, std::size_t len)
{
    char*      out = static_cast<char*>(dest);
    char const* in = static_cast<char const*>(src);
    auto const tab = get_alphabet();

    for(auto n = len / 3; n--;)
    {
        *out++ = tab[ (in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
        *out++ = tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)];
        *out++ = tab[  in[2] & 0x3f];
        in += 3;
    }

    switch(len % 3)
    {
    case 2:
        *out++ = tab[ (in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
        *out++ = tab[                         (in[1] & 0x0f) << 2];
        *out++ = '=';
        break;

    case 1:
        *out++ = tab[ (in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4)];
        *out++ = '=';
        *out++ = '=';
        break;

    case 0:
        break;
    }

    return out - static_cast<char*>(dest);
}

std::string
toBase64(
    std::array<uint8_t, 20> const& v)
{
    std::string s;
    s.resize(encoded_size(v.size()));
    encode(&s[0], v.data(), v.size());
    return s;
}

} // mrdox
} // clang
