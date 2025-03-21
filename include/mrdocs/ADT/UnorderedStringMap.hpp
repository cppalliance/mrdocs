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

#include <unordered_map>
#include <string>

namespace clang::mrdocs {

struct string_hash
{
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;
    std::size_t operator()(const char* str) const        { return hash_type{}(str); }
    std::size_t operator()(std::string_view str) const   { return hash_type{}(str); }
    std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
};

template <class T>
using UnorderedStringMap = std::unordered_map<std::string, T, string_hash, std::equal_to<>>;

} // clang::mrdocs

#endif
