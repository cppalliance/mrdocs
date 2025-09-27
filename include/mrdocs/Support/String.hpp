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
#include <string>
#include <string_view>

namespace mrdocs {

/** Return the substring without leading specified characters.

    @param s The string to trim.
    @param chars The characters to remove.
    @return The modified string.
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

    @param s The string to trim.
    @return The modified string.
 */
constexpr
std::string_view
ltrim(
    std::string_view const s) noexcept
{
    return ltrim(s, " \t\n\v\f\r");
}

/** Return the substring without trailing specified characters.

    @param s The string to trim.
    @param chars The characters to remove.
    @return The modified string.
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

    @param s The string to trim.
    @return The modified string.
 */
constexpr
std::string_view
rtrim(std::string_view const s) noexcept
{
    return rtrim(s, " \t\n\v\f\r");
}

/** Return the substring without leading and trailing horizontal whitespace.

    @param s The string to trim.
    @return The modified string.
 */
constexpr
std::string_view
trim(std::string_view const s) noexcept
{
    return rtrim(ltrim(s));
}

/** Return the substring without leading and trailing specified characters.

    @param s The string to trim.
    @param chars The characters to remove.
    @return The modified string.
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

    @param s The string to trim.
    @param from The substring to remove.
    @param to The substring to replace with.
        If this is empty, the substring is removed.
    @return The modified string.
*/
MRDOCS_DECL
void
replace(std::string& s, std::string_view from, std::string_view to);

/** Determine if a string is only whitespace.

    @param s The string to check.
    @return true if the string is empty or contains only
    whitespace characters (space, tab, newline, vertical tab,
    form feed, carriage return). false otherwise.
 */
constexpr
bool
isWhitespace(std::string_view s) noexcept
{
    return s.find_first_not_of(" \t\n\v\f\r") == std::string::npos;
}

/** Determine if a string starts with one of the specified characters

    @param s The string to check.
    @param chars The characters to check for.
 */
constexpr
bool
startsWithOneOf(std::string_view s, std::string_view chars) noexcept
{
    return !s.empty() && chars.find(s.front()) != std::string_view::npos;
}

/** Determine if a string ends with one of the specified characters

    @param s The string to check.
    @param chars The characters to check for.
 */
constexpr
bool
endsWithOneOf(std::string_view s, std::string_view chars) noexcept
{
    return !s.empty() && chars.find(s.back()) != std::string_view::npos;
}

constexpr
bool
isLowerCase(char const c) noexcept
{
    return c >= 'a' && c <= 'z';
}

constexpr
bool
isLowerCase(std::string_view const s) noexcept
{
    for (char const c : s)
    {
        if (!isLowerCase(c))
        {
            return false;
        }
    }
    return true;
}

constexpr
bool
isUpperCase(char const c) noexcept
{
    return c >= 'A' && c <= 'Z';
}

constexpr
bool
isUpperCase(std::string_view const s) noexcept
{
    for (char const c : s)
    {
        if (!isUpperCase(c))
        {
            return false;
        }
    }
    return true;
}

constexpr
char
toLowerCase(char const c) noexcept
{
    return isUpperCase(c) ? c + ('a' - 'A') : c;
}

constexpr
std::string
toLowerCase(std::string_view const s) noexcept
{
    std::string result;
    result.reserve(s.size());
    for (char const c : s)
    {
        result.push_back(toLowerCase(c));
    }
    return result;
}

constexpr
char
toUpperCase(char const c) noexcept
{
    return isLowerCase(c) ? c - ('a' - 'A') : c;
}

constexpr
std::string
toUpperCase(std::string_view const s) noexcept
{
    std::string result;
    result.reserve(s.size());
    for (char const c : s)
    {
        result.push_back(toUpperCase(c));
    }
    return result;
}

constexpr
bool
isDigit(char const c) noexcept
{
    return c >= '0' && c <= '9';
}

constexpr
bool
isDigit(std::string_view const s) noexcept
{
    for (char const c : s)
    {
        if (!isDigit(c))
        {
            return false;
        }
    }
    return true;
}

constexpr
bool
isAlphabetic(char const c) noexcept
{
    return isLowerCase(c) || isUpperCase(c);
}

constexpr
bool
isAlphabetic(std::string_view const s) noexcept
{
    for (char const c : s)
    {
        if (!isAlphabetic(c))
        {
            return false;
        }
    }
    return true;
}

constexpr
bool
isAlphaNumeric(char const c) noexcept
{
    return isAlphabetic(c) || isDigit(c);
}

constexpr
bool
isAlphaNumeric(std::string_view const s) noexcept
{
    for (char const c : s)
    {
        if (!isAlphaNumeric(c))
        {
            return false;
        }
    }
    return true;
}

constexpr
std::string
toKebabCase(std::string_view const input)
{
    std::string result;
    size_t extraSizeCount = 0;
    for (std::size_t i = 1; i < input.size(); ++i) {
        if (isUpperCase(input[i])) {
            ++extraSizeCount;
        }
    }
    result.reserve(input.size() + extraSizeCount);
    for (size_t i = 0; i < input.size(); ++i) {
        if (char const c = input[i];
            isUpperCase(c))
        {
            if (i != 0) {
                result.push_back('-');
            }
            result.push_back(toLowerCase(c));
        }
        else if (isLowerCase(c) || isDigit(c))
        {
            result.push_back(c);
        }
        else
        {
            result.push_back('-');
        }
    }
    return result;
}

constexpr
std::string
toSnakeCase(std::string_view const input)
{
    std::string result;
    size_t extraSizeCount = 0;
    for (std::size_t i = 1; i < input.size(); ++i) {
        if (isUpperCase(input[i]))
        {
            ++extraSizeCount;
        }
    }
    result.reserve(input.size() + extraSizeCount);
    for (size_t i = 0; i < input.size(); ++i) {
        if (char const c = input[i];
            isUpperCase(c))
        {
            if (i != 0)
            {
                result.push_back('_');
            }
            result.push_back(toLowerCase(c));
        }
        else if (isLowerCase(c) || isDigit(c))
        {
            result.push_back(c);
        }
        else
        {
            result.push_back('_');
        }
    }
    return result;
}

constexpr
std::string
toCamelCase(std::string_view const input)
{
    std::string result;
    result.reserve(input.size());
    bool forceUppercaseNext = false;
    for (char const c : input)
    {
        if (isAlphaNumeric(c))
        {
            if (result.empty())
            {
                result.push_back(toLowerCase(c));
                forceUppercaseNext = false;
            }
            else if (forceUppercaseNext)
            {
                result.push_back(toUpperCase(c));
                forceUppercaseNext = false;
            }
            else
            {
                result.push_back(c);
            }
        }
        else
        {
            forceUppercaseNext = true;
        }
    }
    return result;
}

constexpr
std::string
toPascalCase(std::string_view const input)
{
    std::string result;
    result.reserve(input.size());
    bool forceUppercaseNext = true;
    for (char const c : input)
    {
        if (isAlphaNumeric(c))
        {
            if (forceUppercaseNext)
            {
                result.push_back(toUpperCase(c));
                forceUppercaseNext = false;
            }
            else
            {
                result.push_back(c);
            }
        }
        else
        {
            forceUppercaseNext = true;
        }
    }
    return result;
}


} // mrdocs

#endif
