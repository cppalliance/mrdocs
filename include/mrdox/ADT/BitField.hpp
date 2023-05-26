//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_ADT_BITFIELD_HPP
#define MRDOX_ADT_BITFIELD_HPP

#include <cstdint>
#include <limits>
#include <type_traits>

namespace clang {
namespace mrdox {

constexpr std::uint32_t makeMask(
    unsigned char Offset, unsigned char Size)
{
    auto init = std::numeric_limits<std::uint32_t>::max();
    auto offset = init << Offset;
    auto rest   = init >>= (sizeof(std::uint32_t) * 8) - (Offset + Size);
    return offset & rest;
}

template<unsigned char Offset,
         unsigned char Size = 1u,
         typename T = std::uint32_t>
struct BitField
{
    using underlying_type = std::uint32_t;
    underlying_type value;

    constexpr static underlying_type mask = makeMask(Offset, Size);
    constexpr static underlying_type offset = Offset;
    constexpr static underlying_type size = Size;

    constexpr T get() const noexcept
    {
        constexpr underlying_type simple_mask = makeMask(0u, Offset + Size);
        auto val = value & simple_mask;

        if constexpr (std::is_signed<T>::value && (Size + Offset) < (sizeof(underlying_type) * 8u))
        {
            constexpr underlying_type minus = static_cast<underlying_type>(1) << (Size + Offset);
            if ((val & minus) != static_cast<underlying_type>(0))
                val |= ~simple_mask; //2s complement - I think
        }
        val >>= Offset;
        return static_cast<T>(val);
    }

    constexpr operator T () const noexcept
    {
        return get();
    }

    constexpr void set(T val) noexcept
    {
        auto update = (static_cast<underlying_type>(val) << Offset) & mask;
        value = (value & ~mask) | update;
    }

    constexpr T operator=(T val) noexcept { set(val); return val;}
};

template<unsigned char Offset>
using BitFlag = BitField<Offset, 1u, bool>;

using BitFieldFullValue = BitField<0, 32>;

} // mrdox
} // clang

#endif
