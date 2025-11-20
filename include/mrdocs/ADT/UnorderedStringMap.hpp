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

#include <string>
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

} // mrdocs

#endif // MRDOCS_API_ADT_UNORDEREDSTRINGMAP_HPP
