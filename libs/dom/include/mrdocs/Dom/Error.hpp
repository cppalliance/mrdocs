//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_ERROR_HPP
#define MRDOCS_API_DOM_ERROR_HPP

// dom's std-only error type and Expected alias. dom depends only on the standard
// library and the polyfills library; it never names mrdocs::Error. Errors flow
// UPWARD by implicit conversion: mrdocs::Error has an implicit constructor from
// dom::Error, so consumers convert at the boundary with no caller changes.

#include <mrdocs/polyfill/expected.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mrdocs {
namespace dom {

/** A std-only error value for the dom library (carries a message). */
class Error
{
    std::string message_;
public:
    /** Constructor.

        A default-constructed error carries no message and
        is equivalent to success.
    */
    Error() = default;

    /** Constructor.

        @param message A string describing the failure.
    */
    explicit Error(std::string message) noexcept
        : message_(std::move(message)) {}

    /** Return the error message.

        @return The message describing the failure, or an
        empty string if this error indicates success.
    */
    std::string const& message() const noexcept { return message_; }

    /** Return true if this holds an error.

        @return `true` if the error carries a non-empty message.
    */
    explicit operator bool() const noexcept { return !message_.empty(); }
};

/** dom's result type: the std::expected polyfill, defaulting the error to
    dom::Error (an explicit 2nd argument is allowed, e.g. `Expected<void, E>`). */
template <class T, class E = Error>
using Expected = polyfill::expected<T, E>;

/** Construct a failed @ref Expected value.

    Re-exported from `polyfill` (defined there) so that
    `Unexpected(e)` deduces the error type on every supported compiler.
    Alias-template CTAD (P1814) is not implemented everywhere (e.g. Clang 18).
*/
using polyfill::Unexpected;

} // namespace dom
} // namespace mrdocs

#endif
