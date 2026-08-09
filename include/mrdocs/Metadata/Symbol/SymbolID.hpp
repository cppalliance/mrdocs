//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_SYMBOLID_HPP
#define MRDOCS_API_METADATA_SYMBOL_SYMBOLID_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <compare>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace mrdocs {
class DomCorpus;
namespace dom {
    struct ValueFromTag;
    class Value;
}

/** A unique identifier for a symbol.

    This is calculated as the SHA1 digest of the
    USR. A USRs is a string that provides an
    unambiguous reference to a symbol.
*/
class SymbolID
{
    std::uint8_t data_[20]{};

public:
    /** Sentinel ID used to represent “no symbol.”
    */
    static const SymbolID invalid;

    /** Sentinel ID representing the global namespace.
    */
    static const SymbolID global;

    /** Underlying byte storage.
    */
    using value_type = std::uint8_t;

    /** Construct a zero-initialized identifier.
    */
    constexpr SymbolID() = default;

    /** Construct a SymbolID from a null-terminated string.

        This function constructs a SymbolID from
        a string. The string must be exactly 20
        characters long.

        @param src The string to construct from.
    */
    template <std::convertible_to<value_type> Char>
    constexpr SymbolID(Char const* src)
    {
        [[maybe_unused]] int i = 0;
        for(value_type& c : data_)
        {
            c = *src++;
            ++i;
        }
        MRDOCS_ASSERT(i == 20);
    }

    /** Construct a SymbolID by hashing a string

        @param input The string to hash.
        @return The SymbolID created by hashing the string.
    */
    static
    SymbolID
    createFromString(std::string_view input);

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

    /** Compare two SymbolIDs with strong ordering.
    */
    auto operator<=>(
        SymbolID const& other) const noexcept
    {
        return std::memcmp(
            data(),
            other.data(),
            size()) <=> 0;
    }

    /** Compare two SymbolIDs for equality.
    */
    bool operator==(
        SymbolID const& other) const noexcept = default;
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

/** Convert a SymbolID to a string

    @param id The SymbolID to convert.
    @return The base16 string representation of the SymbolID.
*/
MRDOCS_DECL
std::string
toBase16Str(SymbolID const& id);

/** Convert a SymbolID to its canonical string form.

    This is the representation used for symbol identifiers in the generated
    output (e.g. the XML `id`/`parent` attributes and the DOM `id` field). It
    uses base58, which is compact yet easy to copy, paste, and match with a
    regular expression.

    @param id The SymbolID to convert.
    @return The base58 string representation of the SymbolID.
*/
MRDOCS_DECL
std::string
toBase58Str(SymbolID const& id);

/** Parse the base58 string form of a SymbolID.

    This is the inverse of @ref toBase58Str, used to decode the ids that appear
    in the generated output (e.g. when a template or extension looks a symbol up
    by its id).

    @param str The base58 string to parse.
    @return The SymbolID, or std::nullopt if `str` is not a valid encoding of a
    20-byte SymbolID.
*/
MRDOCS_DECL
std::optional<SymbolID>
fromBase58Str(std::string_view str);

/** Return the result of comparing s0 to s1.

    This function returns true if the string
    s0 is less than the string s1. The comparison
    is first made without regard to case, unless
    the strings compare equal and then they
    are compared with lowercase letters coming
    before uppercase letters.

    @param symbolName0 The first symbol name to compare.
    @param symbolName1 The second symbol name to compare.
    @return The result of the comparison.
*/
MRDOCS_DECL
std::strong_ordering
compareSymbolNames(
    std::string_view symbolName0,
    std::string_view symbolName1) noexcept;

/** Convert SymbolID to dom::Value string in the DOM using toBase58
*/
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SymbolID const& id);


} // mrdocs

template<>
struct std::hash<mrdocs::SymbolID>
{
    std::size_t
    operator()(mrdocs::SymbolID const& id) const noexcept
    {
        return std::hash<std::string_view>()(
            std::string_view(
                reinterpret_cast<char const*>(id.data()), id.size()));
    }
};

#endif // MRDOCS_API_METADATA_SYMBOL_SYMBOLID_HPP
