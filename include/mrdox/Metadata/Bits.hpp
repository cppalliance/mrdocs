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

#ifndef MRDOX_METADATA_BITS_HPP
#define MRDOX_METADATA_BITS_HPP

#include <mrdox/Platform.hpp>
#include <array>
#include <bit> // for constexpr popcount
#include <cassert>
#include <cstdint>
#include <type_traits>

namespace clang {
namespace mrdox {

using BitsValueType = std::uint32_t;

/** A container of packed bits to describe metadata.

    The container may be templated on the
    enumeration, to ensure type safety.
*/
template<class Enum>
class Bits
{
    static_assert(std::is_enum_v<Enum>);
    static_assert(std::is_same_v<
        std::underlying_type_t<Enum>, std::uint32_t>);
#ifdef __cpp_lib_is_scoped_enum
    static_assert(std::is_scoped_enum<Enum>);
#endif

public:
    using value_type = BitsValueType;

    /** Constructor.
    */
    constexpr Bits() noexcept = default;

    /** Return true if all bits are clear.
    */
    constexpr bool
    empty() const noexcept
    {
        return bits_ == 0;
    }

    /** Return a pointer to the array of values.
    */
    constexpr value_type
    value() const noexcept
    {
        return bits_;
    }

    /** Return true or false for a flag.
    */
    template<Enum ID>
    requires (std::has_single_bit(value_type(ID)))
    constexpr bool
    get() const noexcept
    {
        return bits_ & value_type(ID);
    }

    /** Return the value for an id.
    */
    template<Enum ID, class T = value_type>
    requires (std::popcount(value_type(ID)) > 1)
    constexpr T
    get() const noexcept
    {
        return static_cast<T>((bits_ & value_type(ID)) >>
            std::countr_zero(value_type(ID)));
    }

    /** Set a flag to true (or false).
    */
    template<Enum ID>
    requires (std::has_single_bit(value_type(ID)))
    constexpr void
    set(bool value = true) noexcept
    {
        if(value)
            bits_ |= value_type(ID);
        else
            bits_ &= ~value_type(ID);
    }

    /** Set an integer value for the id.
    */
    template<Enum ID, class Integer>
    requires (std::popcount(value_type(ID)) > 1)
    constexpr void
    set(Integer value) noexcept
    {
        assert(static_cast<std::underlying_type_t<Enum>>(ID) > 0);
        assert(value_type(value) < (value_type(ID) >>
                std::countr_zero(value_type(ID))));
        bits_ = (bits_ & ~value_type(ID)) | (
            value_type(value) << std::countr_zero(
                value_type(ID)));
    }

    /** Set all of the bits at once.
    */
    void
    load(value_type bits) noexcept
    {
        bits_ = bits;
    }

    /** Merge other into this.
    */
    constexpr void
    merge(Bits&& other) noexcept
    {
        bits_ |= other.bits_;
    }

private:
    value_type bits_ = 0;
};

//------------------------------------------------

/** Metafunction returns true_type if T is a Bits
*/
/** @{ */
template<class...>
inline constexpr bool is_Bits_v = false;

template<>
inline constexpr bool is_Bits_v<> = true;

template<class Enum0, class... BitsN>
inline constexpr bool is_Bits_v<
    Bits<Enum0>, BitsN...> = is_Bits_v<BitsN...>;
/** @} */

//------------------------------------------------

/** Convert one or more Bits to an array of values.
*/
template<class... BitsN>
requires (is_Bits_v<BitsN...>)
constexpr
std::array<BitsValueType, sizeof...(BitsN)>
getBits(
    BitsN const&... bits) noexcept
{
    return { bits.value()... };
}

template<class... BitsN, std::size_t... Is>
constexpr void BitsHelper(
    std::index_sequence<Is...>,
    std::array<BitsValueType, sizeof...(BitsN)> const& values,
    BitsN&... bits)
{
    (bits.load(values[Is]), ...);
}

/** Load one or more Bits from an array of values.
*/
template<class... BitsN>
requires (is_Bits_v<BitsN...>)
constexpr void
setBits(
    std::array<BitsValueType,
        sizeof...(BitsN)> const& values,
    BitsN&... bits) noexcept
{
    BitsHelper(std::make_index_sequence<
        sizeof...(BitsN)>(), values, bits...);
}

template<class... BitsN>
requires (is_Bits_v<BitsN...>)
constexpr bool
bitsEmpty(BitsN const&... bits) noexcept
{
    return (bits.empty() && ...);
}

} // mrdox
} // clang

#endif
