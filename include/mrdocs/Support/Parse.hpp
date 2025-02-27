//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_PARSE_HPP
#define MRDOCS_API_SUPPORT_PARSE_HPP

#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>

namespace clang::mrdocs {

/** The result of a parse operation.

    This class holds the result of a parse operation.
    The structure is similar to `std::from_chars_result`,
    where we have a `ptr` member that points to the
    first character not parsed, and a `ec` member that
    holds the error code.

    If parsing was successful, then `ec` stores
    a default constructed `Error` object, which
    indicates success. The `operator bool` can
    also be used to check for success.

    The typical format of a parsing function is:

    @code
    ParseResult
    parseType(
        char const* first,
        char const* last,
        Type& value);
    @endcode

    where more parameters can be defined as needed for
    parsing options.
 */
struct ParseResult {
    const char* ptr;
    Error ec;

    friend
    bool
    operator==(
        const ParseResult&,
        const ParseResult& ) = default;

    constexpr
    explicit
    operator bool() const noexcept
    {
        return !ec.failed();
    }
};

/** Concept to determine if there's a parse function for a type.

    This concept checks if a type `T` has a parse function
    with the signature:

    @code
    ParseResult
    parse(
        char const* first,
        char const* last,
        T& value);
    @endcode
 */
template <class T>
concept HasParse = requires(
    char const* first,
    char const* last,
    T& value)
{
    { parse(first, last, value) } -> std::same_as<ParseResult>;
};

/** Parse a string view

    This function parses a string view `sv` into a value
    of type `T`. The function calls the `parse` function
    for the type `T` with the `sv.data()` and `sv.data() + sv.size()`
    as the first and last pointers, respectively.

    If the parse function returns an error, then the function
    returns the error.

    If the parse function returns success, but there are
    characters left in the string view, then the function
    returns an error with the message "trailing characters".

    Otherwise, it returns the value.

    @param sv The string view to parse
    @param value The value to store the result
 */
template <HasParse T>
ParseResult
parse(std::string_view sv, T& value)
{
    ParseResult result = parse(sv.data(), sv.data() + sv.size(), value);
    if (result)
    {
        if (result.ptr != sv.data() + sv.size())
        {
            result.ec = Error("trailing characters");
        }
    }
    return result;
}

/** Parse a string view

    Parse a string view `sv` as an object of type `T`.
    If parsing fails, the function returns an error.

    This overload does not return the `ParseResult` object
    containing the pointer to the first character not parsed.
    Instead, the position of the error is calculated and
    the error message is formatted with the error position.

    @copydetails parse(std::string_view, T&)

 */
template <HasParse T>
Expected<T>
parse(std::string_view sv)
{
    T v;
    ParseResult const result = parse(sv, v);
    if (result)
    {
        return v;
    }
    std::size_t const pos = result.ptr - sv.data();
    return Unexpected(formatError(
        "'{}' at position {}: {}",
        sv, pos, result.ec.reason()));
}

} // clang::mrdocs

#endif