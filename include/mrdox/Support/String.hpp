//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_STRING_HPP
#define MRDOX_API_SUPPORT_STRING_HPP

#include <mrdox/Platform.hpp>
#include <cctype>
#include <string_view>

namespace clang {
namespace mrdox {

/** Return the substring without leading horizontal whitespace.
*/
std::string_view ltrim(std::string_view s) noexcept;

/** Return the substring without trailing horizontal whitespace.
*/
std::string_view rtrim(std::string_view s) noexcept;

/** Return the substring without leading and trailing horizontal whitespace.
*/
std::string_view trim(std::string_view s) noexcept;

inline
std::string_view
ltrim(
    std::string_view s) noexcept
{
    auto it = s.begin();
    for (;; ++it)
    {
        if (it == s.end())
            return {};
        if (! std::isspace(*it))
            break;
    }
    return s.substr(it - s.begin());
}

inline
std::string_view
rtrim(
    std::string_view s) noexcept
{
    auto it = s.end() - 1;
    while(it > s.begin() && std::isspace(*it))
        --it;
    return s.substr(0, it - s.begin());
}

inline
std::string_view
trim(
    std::string_view s) noexcept
{
    auto left = s.begin();
    for (;; ++left)
    {
        if (left == s.end())
            return {};
        if (!isspace(*left))
            break;
    }
    auto right = s.end() - 1;
    while(right > left && std::isspace(*right))
        --right;
    return std::string_view(&*left, right - left + 1);
}

} // mrdox
} // clang

#endif
