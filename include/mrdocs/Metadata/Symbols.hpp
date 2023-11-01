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
    USR. A USRs is a string that provide an
    unambiguous reference to a symbol.
*/
class SymbolID
{
public:
    static const SymbolID invalid;
    static const SymbolID global;

    using value_type = std::uint8_t;

    constexpr SymbolID() = default;

    template<typename Elem>
    constexpr SymbolID(const Elem* src)
    {
        for(auto& c : data_)
            c = *src++;
    }

    explicit operator bool() const noexcept
    {
        return *this != SymbolID::invalid;
    }

    constexpr auto data() const noexcept
    {
        return data_;
    }

    constexpr std::size_t size() const noexcept
    {
        return 20;
    }

    constexpr auto begin() const noexcept
    {
        return data_;
    }

    constexpr auto end() const noexcept
    {
        return data_ + size();
    }

    operator std::string_view() const noexcept
    {
        return std::string_view(reinterpret_cast<
            const char*>(data()), size());
    }

    auto operator<=>(
        const SymbolID& other) const noexcept
    {
        return std::memcmp(
            data(),
            other.data(),
            size()) <=> 0;
    }

    bool operator==(
        const SymbolID& other) const noexcept = default;

private:
    value_type data_[20];
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
