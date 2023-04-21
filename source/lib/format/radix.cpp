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

#include "radix.hpp"
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
    "abcdefghijklmnopqrstuvwxyz"
    "()_-,";

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
toBase64(
    std::array<uint8_t, 20> const& src)
{
    std::string dest;
    dest.resize(base64EncodedSize(src.size()));
    base64Encode(&dest[0], src.data(), src.size());
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

} // mrdox
} // clang
