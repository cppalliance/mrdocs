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

namespace clang::mrdocs {

/** Return the substring without leading specified characters.
 */
constexpr
std::string_view
ltrim(
    std::string_view const s,
    std::string_view const chars) noexcept
{
    return s.substr(std::min(s.find_first_not_of(chars), s.size()));
}

/** Return the substring without leading horizontal whitespace.
 */
constexpr
std::string_view
ltrim(
    std::string_view const s) noexcept
{
    return ltrim(s, " \t\n\v\f\r");
}

/** Return the substring without trailing specified characters.
 */
constexpr
std::string_view
rtrim(
    std::string_view const s,
    std::string_view const chars) noexcept
{
    auto const pos = s.find_last_not_of(chars);
    if (pos == std::string_view::npos)
    {
        return s.substr(0, 0);
    }
    return s.substr(0, pos + 1);
}

/** Return the substring without trailing horizontal whitespace.
 */
constexpr
std::string_view
rtrim(std::string_view const s) noexcept
{
    return rtrim(s, " \t\n\v\f\r");
}

/** Return the substring without leading and trailing horizontal whitespace.
 */
constexpr
std::string_view
trim(std::string_view const s) noexcept
{
    return rtrim(ltrim(s));
}

/** Return the substring without leading and trailing specified characters.
 */
constexpr
std::string_view
trim(
    std::string_view const s,
    std::string_view const chars) noexcept
{
    return rtrim(ltrim(s, chars), chars);
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

/** Determine if a string starts with one of the specified characters
 */
constexpr
bool
startsWithOneOf(std::string_view s, std::string_view chars) noexcept
{
    return !s.empty() && chars.find(s.front()) != std::string_view::npos;
}

/** Determine if a string ends with one of the specified characters
 */
constexpr
bool
endsWithOneOf(std::string_view s, std::string_view chars) noexcept
{
    return !s.empty() && chars.find(s.back()) != std::string_view::npos;
}

} // clang::mrdocs

#endif
