//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_STRING_HPP
#define MRDOCS_API_SUPPORT_STRING_HPP

#include <mrdocs/Platform.hpp>
#include <cctype>
#include <string>
#include <string_view>

namespace clang {
namespace mrdocs {

/** Return the substring without leading horizontal whitespace.
 */
constexpr
std::string_view
ltrim(std::string_view s) noexcept
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

/** Return the substring without trailing horizontal whitespace.
 */
constexpr
std::string_view
rtrim(std::string_view s) noexcept
{
    auto it = s.end() - 1;
    while(it > s.begin() && std::isspace(*it))
    {
        --it;
    }
    return s.substr(0, it - s.begin() + 1);
}

/** Return the substring without leading and trailing horizontal whitespace.
 */
constexpr
std::string_view
trim(std::string_view s) noexcept
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

/** Return the substring without leading and trailing horizontal whitespace.
*/
MRDOCS_DECL
void
replace(std::string& s, std::string_view from, std::string_view to);

/** Determine if a string is only whitespace.
 */
constexpr
bool
isWhitespace(std::string_view s) noexcept
{
    return s.find_first_not_of(" \t\n\v\f\r") == std::string::npos;
}


} // mrdocs
} // clang

#endif
