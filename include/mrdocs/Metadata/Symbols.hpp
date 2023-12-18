//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOLS_HPP
#define MRDOCS_API_METADATA_SYMBOLS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <cstdint>
#include <cstring>
#include <compare>
#include <string_view>

namespace clang {
namespace mrdocs {

/** A unique identifier for a symbol.

    This is calculated as the SHA1 digest of the
    USR. A USRs is a string that provides an
    unambiguous reference to a symbol.
*/
class SymbolID
{
public:
    static const SymbolID invalid;
    static const SymbolID global;

    using value_type = std::uint8_t;

    constexpr SymbolID() = default;

    /** Construct a SymbolID from a null-terminated string.

        This function constructs a SymbolID from
        a string. The string must be exactly 20
        characters long.

        @param src The string to construct from.
    */
    template <std::convertible_to<value_type> Char>
    constexpr SymbolID(const Char* src)
    {
        int i = 0;
        for(value_type& c : data_)
        {
            c = *src++;
            ++i;
        }
        MRDOCS_ASSERT(i == 20);
    }

    /** Return true if this is a valid SymbolID.
     */
    explicit operator bool() const noexcept
    {
        return *this != SymbolID::invalid;
    }

    /** Return the raw data for this SymbolID.
     */
    constexpr auto data() const noexcept
    {
        return data_;
    }

    /** Return the size of the SymbolID.

        The size of a SymbolID is always 20.

     */
    constexpr std::size_t size() const noexcept
    {
        return 20;
    }

    /** Return an iterator to the first byte of the SymbolID.
     */
    constexpr auto begin() const noexcept
    {
        return data_;
    }

    /** Return an iterator to one past the last byte of the SymbolID.
     */
    constexpr auto end() const noexcept
    {
        return data_ + size();
    }

    /** Return a string view of the SymbolID.
     */
    operator std::string_view() const noexcept
    {
        return {reinterpret_cast<
            const char*>(data()), size()};
    }

    /** Compare two SymbolIDs with strong ordering.
     */
    auto operator<=>(
        const SymbolID& other) const noexcept
    {
        return std::memcmp(
            data(),
            other.data(),
            size()) <=> 0;
    }

    /** Compare two SymbolIDs for equality.
     */
    bool operator==(
        const SymbolID& other) const noexcept = default;

private:
    value_type data_[20]{};
};

/** The invalid Symbol ID.
*/
// KRYSTIAN NOTE: msvc requires inline as it doesn't consider this
// to be an inline variable without it (it should; see [dcl.constexpr])
constexpr inline SymbolID SymbolID::invalid = SymbolID();

/** Symbol ID of the global namespace.
*/
constexpr inline SymbolID SymbolID::global =
    "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
    "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF";

/** Return the result of comparing s0 to s1.

    This function returns true if the string
    s0 is less than the string s1. The comparison
    is first made without regard to case, unless
    the strings compare equal and then they
    are compared with lowercase letters coming
    before uppercase letters.
*/
MRDOCS_DECL
std::strong_ordering
compareSymbolNames(
    std::string_view symbolName0,
    std::string_view symbolName1) noexcept;

} // mrdocs
} // clang

template<>
struct std::hash<clang::mrdocs::SymbolID>
{
    std::size_t operator()(
        const clang::mrdocs::SymbolID& id) const
    {
        return std::hash<std::string_view>()(
            std::string_view(id));
    }
};

#endif
