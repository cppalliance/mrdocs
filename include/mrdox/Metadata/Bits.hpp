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
#include <mrdox/Debug.hpp>
#include <bit> // for constexpr popcount
#include <cstdint>
#include <type_traits>

namespace clang {
namespace mrdox {

/** A container of packed bits to describe metadata.

    The container may be templated on the enumeration,
    this ensures type safety.
*/
template<class... Enums>
class Bits
{
#if 0
    static_assert(std::is_enum_v<Enum>);
    static_assert(std::is_same_v<
        std::underlying_type_t<Enum>, std::uint32_t>);
#ifdef __cpp_lib_is_scoped_enum
    static_assert(std::is_scoped_enum<Enum>);
#endif
#endif
    template<class T>
    static constexpr void count()
    {
        //static_assert(false, "enum not found");
    }

    template<class T, class E0, class... En>
    static constexpr int count()
    {
        if constexpr(std::is_same_v<T, E0>)
            return 0;
        else
            return 1 + count<T, En...>();
    }

public:
    using value_type = std::uint32_t;

    /** Constructor.
    */
    constexpr Bits() noexcept = default;

    /** Return the size of the integer array representing the bits.
    */
    static constexpr std::size_t
    size()
    {
        return sizeof...(Enums);
    }

    /** Return the index of the specified enum E in the pack.
    */
    template<class E>
    static constexpr int
    indexof()
    {
        return count<E, Enums...>();
    }

    /** Return a pointer to the beginning of the array of values.
    */
    constexpr value_type const* data() const
    {
        return bits_;
    }

    /** Set a flag to true (or false).
    */
    template<class Enum>
    constexpr void
    set(Enum id, bool value = true) noexcept
    {
        Assert(std::has_single_bit(value_type(id)));
        constexpr auto I = indexof<Enum>();
        if(value)
            bits_[I] |= value_type(id);
        else
            bits_[I] &= ~value_type(id);
    }

    /** Set an integer value for the id.
    */
    template<class Enum, class Integer>
    constexpr void
    set(Enum id, Integer value) noexcept
    {
        Assert(std::popcount(value_type(id)) > 1);
        Assert(value > 0);
        Assert(value_type(value) < (
            value_type(id) >>
                std::countr_zero(value_type(id))));
        constexpr auto I = indexof<Enum>();
        bits_[I] = (bits_[I] & ~value_type(id)) | (
            value_type(value) << std::countr_zero(value_type(id)));
    }

    /** Return true or false for a flag.
    */
    template<class Enum>
    constexpr bool
    get(Enum id) const noexcept
    {
        Assert(std::has_single_bit(value_type(id)));
        constexpr auto I = indexof<Enum>();
        return bits_[I] & value_type(id);
    }

    /** Return the value for an id.
    */
    template<class Enum>
    constexpr value_type
    get(Enum id) const noexcept
    {
        Assert(std::popcount(value_type(id)) > 1);
        constexpr auto I = indexof<Enum>();
        return (bits_[I] & value_type(id)) >>
            std::countr_zero(value_type(id));
    }

    void merge(Bits&& other) noexcept;

private:
    value_type bits_[size()]{};
};

} // mrdox
} // clang

#endif
