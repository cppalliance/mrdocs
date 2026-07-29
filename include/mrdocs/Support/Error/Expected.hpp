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

#ifndef MRDOCS_API_SUPPORT_ERROR_EXPECTED_HPP
#define MRDOCS_API_SUPPORT_ERROR_EXPECTED_HPP

// The std::expected polyfill lives in libs/polyfill (mrdocs::polyfill, std-only).
// This header is the mrdocs-specific wrapper: it supplies the default `Error`
// type via the `Expected` alias, and keeps the Error-coupled failed/error
// helpers and the MRDOCS_TRY/MRDOCS_CHECK macros (extensions).

#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/polyfill/expected.hpp>
#include <type_traits>
#include <utility>

namespace mrdocs {

/** The mrdocs result type: `polyfill::expected` defaulting the error to `Error`.

    A thin alias over the standard-library `std::expected` polyfill that supplies
    the mrdocs `Error` type as the default error, so most code can write
    `Expected<T>`.
*/
template <class T, class E = Error>
using Expected = polyfill::expected<T, E>;

/** Construct an unexpected (error) value, as passed to @ref Expected.

    Re-exported from `polyfill` (defined there) so that
    `Unexpected(e)` deduces the error type on every supported compiler.
    Alias-template CTAD (P1814) is not implemented everywhere (e.g. Clang 18),
    which would otherwise reject the deduced form at hundreds of call sites.
*/
using polyfill::Unexpected;

/// Exception thrown on bad @ref Expected access. @copydoc polyfill::bad_expected_access
template <class E>
using BadExpectedAccess = polyfill::bad_expected_access<E>;

/// Tag type used to construct an @ref Expected in the error state.
using polyfill::unexpect_t;

/// Tag value of type @ref unexpect_t.
using polyfill::unexpect;

namespace detail
{
    // Re-export the polyfill trait so mrdocs::detail::isExpected users (Dom,
    // and the failed/error helpers below) keep resolving.
    using polyfill::detail::isExpected;

    template <class T>
    constexpr
    bool
    failed(T const&t)
    {
        if constexpr (isExpected<std::decay_t<T>>)
        {
            return !t;
        }
        else if constexpr (std::same_as<std::decay_t<T>, Error>)
        {
            return t.failed();
        }
        else if constexpr (requires (T const& t0) { t0.empty(); })
        {
            return t.empty();
        }
        else if constexpr (std::constructible_from<bool, T>)
        {
            return !t;
        }
        else
        {
            return false;
        }
    }

    template <class T>
    constexpr
    decltype(auto)
    error(T const& t)
    {
        if constexpr (isExpected<std::decay_t<T>>)
        {
            return t.error();
        }
        else if constexpr (std::same_as<std::decay_t<T>, Error>)
        {
            return t;
        }
        else if constexpr (requires (T const& t0) { t0.empty(); })
        {
            return Error("Empty value");
        }
        else if constexpr (std::constructible_from<bool, T>)
        {
            return Error("Invalid value");
        }
    }
}

#ifndef MRDOCS_TRY
#    define MRDOCS_MERGE_(a, b) a##b
#    define MRDOCS_LABEL_(a)    MRDOCS_MERGE_(expected_result_, a)
#    define MRDOCS_UNIQUE_NAME  MRDOCS_LABEL_(__LINE__)

// `detail::failed` and `detail::error` below are qualified with `::mrdocs::`
// so the macros remain correct when expanded inside another `detail`
// namespace (e.g. `mrdocs::lua::detail`): a qualified `detail::` lookup
// stops at the first matching nested `detail` and never falls through to
// `mrdocs::detail`. `Unexpected` and `Error` are left unqualified: ordinary
// scope walking finds them in `mrdocs::`.

/// Try to retrive expected-like type
#    define MRDOCS_TRY_VOID(expr)                          \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (::mrdocs::detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(::mrdocs::detail::error(MRDOCS_UNIQUE_NAME)); \
        }                                                 \
        void(0)
#    define MRDOCS_TRY_VAR(var, expr)                      \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (::mrdocs::detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(::mrdocs::detail::error(MRDOCS_UNIQUE_NAME)); \
        }                                                  \
        var = *std::move(MRDOCS_UNIQUE_NAME)
#    define MRDOCS_TRY_MSG(var, expr, msg)                 \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (::mrdocs::detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(Error(msg));                 \
        }                                                  \
        var = *std::move(MRDOCS_UNIQUE_NAME)
#    define MRDOCS_TRY_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#    define MRDOCS_TRY(...) \
        MRDOCS_TRY_GET_MACRO(__VA_ARGS__, MRDOCS_TRY_MSG, MRDOCS_TRY_VAR, MRDOCS_TRY_VOID)(__VA_ARGS__)

// Unwrap an expected-like result into a structured binding. The binding
// names are wrapped in parentheses so the comma between them is not seen as
// a macro-argument separator, e.g.
// `MRDOCS_TRY_BIND((a, b), makePair())`.
#    define MRDOCS_UNPAREN_(...) __VA_ARGS__
#    define MRDOCS_UNPAREN(x)    MRDOCS_UNPAREN_ x
#    define MRDOCS_TRY_BIND(names, expr)                   \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (::mrdocs::detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(::mrdocs::detail::error(MRDOCS_UNIQUE_NAME)); \
        }                                                  \
        auto [MRDOCS_UNPAREN(names)] = *std::move(MRDOCS_UNIQUE_NAME)

/// Check existing expected-like type
#    define MRDOCS_CHECK_VOID(var)                         \
        if (::mrdocs::detail::failed(var)) {                         \
            return Unexpected(::mrdocs::detail::error(var));         \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_MSG(var, msg)                     \
        if (::mrdocs::detail::failed(var)) {                         \
            return Unexpected(Error(msg));                 \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_GET_MACRO(_1, _2, NAME, ...) NAME
#    define MRDOCS_CHECK(...) \
        MRDOCS_CHECK_GET_MACRO(__VA_ARGS__, MRDOCS_CHECK_MSG, MRDOCS_CHECK_VOID)(__VA_ARGS__)

/// Check existing expected-like type and return custom value otherwise
#    define MRDOCS_CHECK_OR_VOID(var)                      \
        if (::mrdocs::detail::failed(var)) {                         \
            return;                                        \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_OR_VALUE(var, value)              \
        if (::mrdocs::detail::failed(var)) {                         \
            return value;                                  \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_GET_OR_MACRO(_1, _2, NAME, ...) NAME
#    define MRDOCS_CHECK_OR(...) \
        MRDOCS_CHECK_GET_OR_MACRO(__VA_ARGS__, MRDOCS_CHECK_OR_VALUE, MRDOCS_CHECK_OR_VOID)(__VA_ARGS__)

#    define MRDOCS_CHECK_OR_CONTINUE(var)                  \
        if (::mrdocs::detail::failed(var)) {                         \
            continue;                                      \
        }                                                  \
        void(0)

#    define MRDOCS_CHECK_OR_BREAK(var)     \
        if (::mrdocs::detail::failed(var)) \
        {                                  \
            break;                         \
        }                                  \
        void(0)

#endif

} // namespace mrdocs

#endif
