//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_STRING_STRING_HPP
#define MRDOCS_API_SUPPORT_STRING_STRING_HPP

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

/** Determine if a character is whitespace.

    @param c Character to inspect.
    @return `true` if `c` is a horizontal or vertical whitespace character.
*/
constexpr
bool
isWhitespace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\v' || c == '\f' || c == '\r';
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

/** Determine if a character is a lowercase ASCII letter.

    @param c Character to inspect.
    @return `true` if `c` is in the range 'a' to 'z'.
*/
constexpr
bool
isLowerCase(char const c) noexcept
{
    return c >= 'a' && c <= 'z';
}

/** Determine if every character in a string is lowercase ASCII.

    @param s String to inspect.
    @return `true` if all characters are lowercase ASCII letters.
*/
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

/** Determine if a character is an uppercase ASCII letter.

    @param c Character to inspect.
    @return `true` if `c` is in the range 'A' to 'Z'.
*/
constexpr
bool
isUpperCase(char const c) noexcept
{
    return c >= 'A' && c <= 'Z';
}

/** Determine if every character in a string is uppercase ASCII.

    @param s String to inspect.
    @return `true` if all characters are uppercase ASCII letters.
*/
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

/** Convert a character to lowercase ASCII without locale.

    @param c Character to convert.
    @return Lowercase version of `c` if it is uppercase; otherwise `c`.
*/
constexpr
char
toLowerCase(char const c) noexcept
{
    return isUpperCase(c) ? static_cast<char>(c - 'A' + 'a') : c;
}

/** Return a lowercase copy of the string without locale.

    @param s Input string.
    @return Lowercase copy of `s`.
*/
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

/** Compare two strings for case-insensitive ASCII equality.

    @param a First string.
    @param b Second string.
    @return `true` if `a` and `b` are equal ignoring ASCII case.
*/
constexpr
bool
ciEqual(std::string_view const a, std::string_view const b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (toLowerCase(a[i]) != toLowerCase(b[i]))
        {
            return false;
        }
    }
    return true;
}

/** Convert a character to uppercase ASCII without locale.

    @param c Character to convert.
    @return Uppercase version of `c` if it is lowercase; otherwise `c`.
*/
constexpr
char
toUpperCase(char const c) noexcept
{
    return isLowerCase(c) ? static_cast<char>(c - 'a' + 'A') : c;
}

/** Return an uppercase copy of the string without locale.

    @param s Input string.
    @return Uppercase copy of `s`.
*/
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

/** Determine if a character is an ASCII digit.

    @param c Character to inspect.
    @return `true` if `c` is between '0' and '9'.
*/
constexpr
bool
isDigit(char const c) noexcept
{
    return c >= '0' && c <= '9';
}

/** Determine if every character in a string is an ASCII digit.

    @param s String to inspect.
    @return `true` if all characters are digits.
*/
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

/** Determine if a character is an ASCII letter.

    @param c Character to inspect.
    @return `true` if `c` is in the ranges 'a'-'z' or 'A'-'Z'.
*/
constexpr
bool
isAlphabetic(char const c) noexcept
{
    return isLowerCase(c) || isUpperCase(c);
}

/** Determine if every character in a string is an ASCII letter.

    @param s String to inspect.
    @return `true` if all characters are alphabetic.
*/
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

/** Determine if a character is ASCII alphanumeric.

    @param c Character to inspect.
    @return `true` if `c` is an ASCII letter or digit.
*/
constexpr
bool
isAlphaNumeric(char const c) noexcept
{
    return isAlphabetic(c) || isDigit(c);
}

/** Determine if every character in a string is ASCII alphanumeric.

    @param s String to inspect.
    @return `true` if all characters are ASCII letters or digits.
*/
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

/** Convert a string to `kebab-case` using ASCII letter rules.

    @param input Source string.
    @return New string converted to kebab-case.
*/
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

/** Convert a string to `snake_case` using ASCII letter rules.

    @param input Source string.
    @return New string converted to snake_case.
*/
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

/** Convert a string to `camelCase` using ASCII letter rules.

    @param input Source string.
    @return New string converted to camelCase.
*/
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

/** Convert a string to `PascalCase` using ASCII letter rules.

    @param input Source string.
    @return New string converted to PascalCase.
*/
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

/** Reindent code by removing the common leading spaces and adding the specified indent.

    @param code The code block to unindent.
    @param indent The number of spaces to insert in front of each line after trimming.
    @return The modified code block.
*/
MRDOCS_DECL
std::string
reindentCode(std::string_view code, std::size_t indent = 0);

} // mrdocs

#endif
