//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_ERROR_HPP
#define MRDOCS_API_HANDLEBARS_ERROR_HPP

// handlebars' error type and Expected alias. The handlebars library depends
// only on the standard library, the polyfills library, and dom; it never names
// mrdocs::Error. Errors flow UPWARD by implicit conversion: dom::Error converts
// to handlebars::Error here, and mrdocs::Error has an implicit constructor from
// handlebars::Error, so consumers convert at the boundary with no caller changes.

#include <mrdocs/Dom/Error.hpp>
#include <mrdocs/polyfill/expected.hpp>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mrdocs {

/** The Handlebars template engine.

    Types and functions implementing the Handlebars template language: the
    @ref Handlebars environment, its built-in helpers, and the @ref Error type
    it reports failures with.
*/
namespace handlebars {

/** An error thrown or returned by Handlebars.

    An error returned or thrown by the Handlebars environment when an error
    occurs during template rendering. The message matches the error message
    returned by Handlebars.js, and the object also carries the line, column,
    and position of the error in the template so callers can produce more
    detailed diagnostics.
*/
struct Error
    : public std::runtime_error
{
    /** Line number in the template where the error occurred.
    */
    std::size_t line = static_cast<std::size_t>(-1);
    /** Column of the error in the template.
    */
    std::size_t column = static_cast<std::size_t>(-1);
    /** Absolute character position of the error.
    */
    std::size_t pos = static_cast<std::size_t>(-1);

    /** Construct an error with a message.
        @param msg Description of the failure.
    */
    Error(std::string_view msg)
        : std::runtime_error(std::string(msg)) {}

    /** Construct an error from a dom error.

        Implicit on purpose: errors flow upward from the std-only `dom::Error`
        to `handlebars::Error` at the library boundary.

        @param e The dom error to adopt.
    */
    Error(dom::Error const& e)
        : std::runtime_error(std::string(e.message())) {}

    /** Construct an error with location information.
        @param msg Description of the failure.
        @param line_ Line number where it occurred.
        @param column_ Column number where it occurred.
        @param pos_ Absolute character position.
    */
    Error(std::string_view msg, std::size_t line_,
                    std::size_t column_, std::size_t pos_)
        : std::runtime_error(std::format("{} - {}:{}", msg, line_, column_)),
          line(line_), column(column_), pos(pos_) {}

    /** Return the error message.
        @return The message describing the failure.
    */
    std::string_view message() const noexcept { return what(); }
};

/** handlebars' result type: the std::expected polyfill, defaulting the error
    to handlebars::Error (an explicit 2nd argument is allowed). */
template <class T, class E = Error>
using Expected = polyfill::expected<T, E>;

/** Construct a failed @ref Expected value.

    Re-exported from `polyfill` (defined there) so that
    `Unexpected(e)` deduces the error type on every supported compiler.
    Alias-template CTAD (P1814) is not implemented everywhere (e.g. Clang 18).
*/
using polyfill::Unexpected;

} // namespace handlebars


} // namespace mrdocs

#endif
