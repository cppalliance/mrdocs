//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ADT_UNORDEREDSTRINGMAP_HPP
#define MRDOCS_API_ADT_UNORDEREDSTRINGMAP_HPP

#include <mrdocs/Support/String/String.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mrdocs {

/** Transparent hash functor for string-like keys.
*/
struct StringHash
{
    /** Underlying hash implementation.
    */
    using hash_type = std::hash<std::string_view>;
    /** Marker for heterogeneous lookup.
    */
    using is_transparent = void;
    /** Hash C-string input.
        @return Hash value of the string.
    */
    std::size_t operator()(const char* str) const        { return hash_type{}(str); }
    /** Hash string_view input.
        @return Hash value of the string view.
    */
    std::size_t operator()(std::string_view str) const   { return hash_type{}(str); }
    /** Hash std::string input.
        @return Hash value of the string.
    */
    std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
};

/** unordered_map keyed by std::string with transparent hashing.
*/
template <class T>
using UnorderedStringMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

/** unordered_multimap keyed by std::string with transparent hashing.
*/
template <class T>
using UnorderedStringMultiMap = std::unordered_multimap<std::string, T, StringHash, std::equal_to<>>;

/** Transparent case-insensitive hash functor for string-like keys.

    Folds each character to lowercase while hashing, so keys that differ only
    in ASCII case hash to the same bucket. No copy of the key is made.
*/
struct CIStringHash
{
    /** Marker for heterogeneous lookup.
    */
    using is_transparent = void;
    /** Hash a string view, ignoring ASCII case.
        @return Case-insensitive hash value of the string.
    */
    std::size_t operator()(std::string_view const str) const noexcept
    {
        // FNV-1a over the lowercased bytes.
        std::size_t h = 14695981039346656037ULL;
        for (char const c : str)
        {
            h ^= static_cast<unsigned char>(toLowerCase(c));
            h *= 1099511628211ULL;
        }
        return h;
    }
    /** @copydoc operator()(std::string_view) const */
    std::size_t operator()(char const* str) const noexcept
    {
        return operator()(std::string_view(str));
    }
    /** @copydoc operator()(std::string_view) const */
    std::size_t operator()(std::string const& str) const noexcept
    {
        return operator()(std::string_view(str));
    }
};

/** Transparent case-insensitive equality functor for string-like keys.
*/
struct CIStringEqual
{
    /** Marker for heterogeneous lookup.
    */
    using is_transparent = void;
    /** Compare two string views for case-insensitive equality.
        @param a The first string view to compare.
        @param b The second string view to compare.
        @return `true` if the arguments are equal ignoring ASCII case.
    */
    bool operator()(std::string_view const a, std::string_view const b) const noexcept
    {
        return ciEqual(a, b);
    }
};

/** unordered_multimap keyed by std::string, matching keys case-insensitively.
*/
template <class T>
using UnorderedCIStringMultiMap = std::unordered_multimap<std::string, T, CIStringHash, CIStringEqual>;

} // mrdocs

#endif // MRDOCS_API_ADT_UNORDEREDSTRINGMAP_HPP
