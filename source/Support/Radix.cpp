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

#include "Support/Radix.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Platform.hpp>
#include <llvm/ADT/StringExtras.h>
#include <algorithm>
#include <vector>

namespace clang {
namespace mrdox {

namespace {

// https://medium.com/@thanhdonguyen01/fast-implementation-of-big-integers-in-c-part-1-2cc32bd577a3

template<class UIntTy>
class BigInteger
{
    static_assert(
        std::is_unsigned_v<UIntTy> &&
        std::is_integral_v<UIntTy>);
    static_assert(
        sizeof(std::uint64_t) >= sizeof(UIntTy));
public:
    static constexpr std::size_t Base = sizeof(UIntTy) << 8;

    BigInteger() = default;
    BigInteger(BigInteger const&) = default;
    BigInteger& operator=(BigInteger const&) = default;

    BigInteger(UIntTy v)
    {
        if(v)
            digits_.push_back(v);
    }

    BigInteger(
        UIntTy const* data,
        std::size_t size)
        : digits_(data, data + size)
    {
        reverse();
        remove_zeros();
        reverse();
    }

    bool
    isZero() const noexcept
    {
        return digits_.empty();
    }

    BigInteger
    div(UIntTy& remainder, UIntTy digit)
    {
        if(digits_.empty())
            return 0;
        remainder = digits_.back() % digit;
        return *this / digit;
    }

    friend
    BigInteger
    operator/(
        BigInteger a, UIntTy b)
    {
        BigInteger res;
        std::size_t i = 0;
        std::uint64_t sum = 0;
        for(; i < a.digits_.size(); i++)
        {
            sum = sum * Base + a.digits_[i];
            res.digits_.push_back(sum / b);
            sum -= res.digits_.back() * b;
        }
        res.reverse();
        res.remove_zeros();
        res.reverse();
        return res;
    }

private:
    void insert_a_zero()
    {
        digits_.push_back(0);
    }
    void insert_zeros(std::size_t new_size)
    {
        while (digits_.size() < new_size)
            insert_a_zero();
    }
    void remove_zeros()
    {
        while (digits_.back() == 0)
            digits_.pop_back();
    }
    void reverse() noexcept
    {
        std::reverse(
            digits_.begin(), digits_.end());
    }

    std::vector<UIntTy> digits_;
};

//------------------------------------------------

static constexpr char baseFNDigits[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz";

static constexpr std::size_t baseFN = sizeof(baseFNDigits) - 1;

static
std::size_t constexpr
baseFNEncodedSize(
    std::size_t n)
{
    return (n * 256 + baseFN - 1) / baseFN;
}

/** Encode a series of octets as a padded, base36 string.
    The resulting string will not be null terminated.

    @par Requires
    The memory pointed to by `out` points to valid memory
    of at least `encoded_size(len)` bytes.
    @return The number of characters written to `out`. This
    will exclude any null termination.
*/
static
std::size_t
baseFNEncode(
    void* dest,
    void const* src,
    std::size_t len)
{
    auto out = static_cast<char*>(dest);
    BigInteger<std::uint8_t> N(
        static_cast<std::uint8_t const*>(src), len);
    while(! N.isZero())
    {
        std::uint8_t rem;
        N = N.div(rem, baseFN);
        *out++ = baseFNDigits[rem];
    }
    return out - static_cast<char*>(dest);
}

//------------------------------------------------

static
std::size_t constexpr
base64EncodedSize(
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
base64Encode(void* dest, void const* src, std::size_t len)
{
    char*      out = static_cast<char*>(dest);
    char const* in = static_cast<char const*>(src);
    static char constexpr tab[] = {
        "ABCDEFGHIJKLMNOP"
        "QRSTUVWXYZabcdef"
        "ghijklmnopqrstuv"
        "wxyz0123456789+/"
    };

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

} // (anon)

//------------------------------------------------

std::string
toBase64(std::string_view str)
{
    std::string dest;
    dest.resize(base64EncodedSize(str.size()));
    base64Encode(&dest[0], str.data(), str.size());
    return dest;
}

llvm::StringRef
toBaseFN(
    llvm::SmallVectorImpl<char>& dest,
    llvm::ArrayRef<uint8_t> src)
{
    dest.resize(baseFNEncodedSize(src.size()));
    auto n = baseFNEncode(&dest[0], src.data(), src.size());
    return llvm::StringRef(dest.data(), n);
}

std::string_view
toBase32(
    std::string& dest,
    std::string_view src)
{
#if 0
    std::vector<std::uint8_t> v;
    v.reserve(2 * ((binaryString.size() + 14) / 15));
    while(binaryString.size() >= 15)
    {
        std::uint16_t u =
            (binaryString[ 0]-'0') * 0x0001 +
            (binaryString[ 1]-'0') * 0x0002 +
            (binaryString[ 2]-'0') * 0x0004 +
            (binaryString[ 3]-'0') * 0x0008 +
            (binaryString[ 4]-'0') * 0x0010 +
            (binaryString[ 5]-'0') * 0x0020 +
            (binaryString[ 6]-'0') * 0x0040 +
            (binaryString[ 7]-'0') * 0x0080 +
            (binaryString[ 8]-'0') * 0x0100 +
            (binaryString[ 9]-'0') * 0x0200 +
            (binaryString[10]-'0') * 0x0400 +
            (binaryString[11]-'0') * 0x0800 +
            (binaryString[12]-'0') * 0x1000 +
            (binaryString[13]-'0') * 0x2000 +
            (binaryString[14]-'0') * 0x4000;
        v.push_back( u & 0x00ff);
        v.push_back((u & 0xff00) >> 8);
        binaryString = binaryString.substr(15);
    }
    if(! binaryString.empty())
    {
        char temp[15] = {
            '0', '0', '0', '0', '0', '0', '0',
            '0', '0', '0', '0', '0', '0', '0', '0' };
        std::memcpy(temp, binaryString.data(), binaryString.size());
        std::uint16_t u =
            (temp[ 0]-'0') * 0x0001 +
            (temp[ 1]-'0') * 0x0002 +
            (temp[ 2]-'0') * 0x0004 +
            (temp[ 3]-'0') * 0x0008 +
            (temp[ 4]-'0') * 0x0010 +
            (temp[ 5]-'0') * 0x0020 +
            (temp[ 6]-'0') * 0x0040 +
            (temp[ 7]-'0') * 0x0080 +
            (temp[ 8]-'0') * 0x0100 +
            (temp[ 9]-'0') * 0x0200 +
            (temp[10]-'0') * 0x0400 +
            (temp[11]-'0') * 0x0800 +
            (temp[12]-'0') * 0x1000 +
            (temp[13]-'0') * 0x2000 +
            (temp[14]-'0') * 0x4000;
        v.push_back( u & 0x00ff);
        v.push_back((u & 0xff00) >> 8);
    }
    dest.clear();
    MRDOX_ASSERT((v.size() & 1) == 0);
    dest.reserve(3 * (v.size() / 2));
    auto it = v.data();
    auto const end = it + v.size();
    while(it != end)
    {
        static constexpr char alphabet[33] =
            "012345abcdefghijklmnopqrstuvwxyz";
        std::uint16_t t = 256 * it[1] + it[0];
        dest.push_back(alphabet[(t & 0x001f)]);
        dest.push_back(alphabet[(t & 0x03e0) >> 5]);
        dest.push_back(alphabet[(t & 0x7c00) >> 10]);
        it += 2;
    }
    while(! dest.empty())
        if(dest.back() == '0')
            dest.pop_back();
        else
            break;
#endif
    return dest;
}

std::string
toBase16(
    std::string_view str,
    bool lowercase)
{
    return llvm::toHex(
        llvm::StringRef(str.data(), str.size()),
        lowercase);
}

} // mrdox
} // clang
