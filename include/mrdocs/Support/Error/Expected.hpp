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
#    define MRDOCS_DETAIL_MERGE(a, b) a##b
#    define MRDOCS_DETAIL_LABEL(a)    MRDOCS_DETAIL_MERGE(expected_result_, a)
#    define MRDOCS_DETAIL_UNIQUE_NAME MRDOCS_DETAIL_LABEL(__LINE__)

// `detail::failed` and `detail::error` below are qualified with `::mrdocs::`
// so the macros remain correct when expanded inside another `detail`
// namespace (e.g. `mrdocs::lua::detail`): a qualified `detail::` lookup
// stops at the first matching nested `detail` and never falls through to
// `mrdocs::detail`. `Unexpected` and `Error` are left unqualified: ordinary
// scope walking finds them in `mrdocs::`.

#    define MRDOCS_DETAIL_TRY_VOID(expr)                   \
        auto MRDOCS_DETAIL_UNIQUE_NAME = expr;             \
        if (::mrdocs::detail::failed(MRDOCS_DETAIL_UNIQUE_NAME)) {   \
            return Unexpected(::mrdocs::detail::error(MRDOCS_DETAIL_UNIQUE_NAME)); \
        }                                                 \
        void(0)
#    define MRDOCS_DETAIL_TRY_VAR(var, expr)               \
        auto MRDOCS_DETAIL_UNIQUE_NAME = expr;             \
        if (::mrdocs::detail::failed(MRDOCS_DETAIL_UNIQUE_NAME)) {   \
            return Unexpected(::mrdocs::detail::error(MRDOCS_DETAIL_UNIQUE_NAME)); \
        }                                                  \
        var = *std::move(MRDOCS_DETAIL_UNIQUE_NAME)
#    define MRDOCS_DETAIL_TRY_MSG(var, expr, msg)          \
        auto MRDOCS_DETAIL_UNIQUE_NAME = expr;             \
        if (::mrdocs::detail::failed(MRDOCS_DETAIL_UNIQUE_NAME)) {   \
            return Unexpected(Error(msg));                 \
        }                                                  \
        var = *std::move(MRDOCS_DETAIL_UNIQUE_NAME)
#    define MRDOCS_DETAIL_TRY_SELECT(_1, _2, _3, NAME, ...) NAME

/** Evaluate an expected-like expression and propagate its error.

    The expression is evaluated once and stored in a hidden local. If it
    failed, the enclosing function returns `Unexpected` with its error.
    Otherwise the value is moved into the variable you name, if any. The
    enclosing function must return an @ref mrdocs::Expected (or anything
    that can be constructed from `Unexpected`).

    A value counts as failed when it is:

    @li an @ref mrdocs::Expected without a value (its error is propagated),
    @li a failed @ref mrdocs::Error (the error itself is propagated),
    @li anything with an `empty()` member that returns `true` (the error
        is `Error("Empty value")`),
    @li anything that converts to `false` (the error is
        `Error("Invalid value")`).

    Values that match none of these never count as failed.

    The macro takes one, two, or three arguments. The equivalent code
    below is simplified: the hidden local is really named
    `expected_result_<line>`, and `failed`/`error` are
    `::mrdocs::detail::failed` and `::mrdocs::detail::error`.

    With one argument, it checks the expression and discards the value:

    @code
    MRDOCS_TRY(checkReadable(path));
    @endcode

    is equivalent to:

    @code
    auto tmp = checkReadable(path);
    if (failed(tmp)) {
        return Unexpected(error(tmp));
    }
    @endcode

    This form also works on values that aren't an @ref mrdocs::Expected.
    An empty container fails with `"Empty value"`, and a value that
    converts to `false` fails with `"Invalid value"`:

    @code
    MRDOCS_TRY(findInputs(dir));  // a container
    MRDOCS_TRY(isConfigured());   // a bool
    @endcode

    With two arguments, it declares a variable and moves the value into
    it:

    @code
    MRDOCS_TRY(std::string text, readFile(path));
    @endcode

    is equivalent to:

    @code
    auto tmp = readFile(path);
    if (failed(tmp)) {
        return Unexpected(error(tmp));
    }
    std::string text = *std::move(tmp);
    @endcode

    The first argument can also name a variable that already exists, in
    which case the value is assigned to it:

    @code
    MRDOCS_TRY(text, trim(text));
    @endcode

    is equivalent to:

    @code
    auto tmp = trim(text);
    if (failed(tmp)) {
        return Unexpected(error(tmp));
    }
    text = *std::move(tmp);
    @endcode

    With three arguments, it works like the two-argument form but
    replaces the error with your own message:

    @code
    MRDOCS_TRY(int n, parseInt(text), "count isn't a number");
    @endcode

    is equivalent to:

    @code
    auto tmp = parseInt(text);
    if (failed(tmp)) {
        return Unexpected(Error("count isn't a number"));
    }
    int n = *std::move(tmp);
    @endcode

    Things to keep in mind:

    @li It expands to several statements, so use it only in a function
        body, and put braces around it when it's the body of an `if`,
        `else`, or loop.
    @li The hidden local is named after `__LINE__`, so you can't use it
        twice on the same line (or twice inside one line of another
        macro).
    @li Like any statement, it needs a trailing semicolon.
    @li The expression is stored with `auto`, so an lvalue argument is
        copied, not moved.
    @li The two- and three-argument forms read the value with `*`, so
        they need a type with a value to dereference, such as
        `Expected<T>`. Use the one-argument form for `Expected<void>`,
        @ref mrdocs::Error, containers, and `bool`.
    @li The three-argument form discards the original error and returns
        `Error(msg)`, so `msg` can be anything @ref mrdocs::Error is
        constructible from, such as a non-empty string. It's only
        evaluated on failure. `Error` is named unqualified, so it needs
        `mrdocs::Error` to be visible (inside namespace `mrdocs`, or
        after `using mrdocs::Error;`).

    To unpack the value into several names, use `MRDOCS_TRY_BIND`.
*/
#    define MRDOCS_TRY(...) \
        MRDOCS_DETAIL_TRY_SELECT(__VA_ARGS__, MRDOCS_DETAIL_TRY_MSG, MRDOCS_DETAIL_TRY_VAR, MRDOCS_DETAIL_TRY_VOID)(__VA_ARGS__)

#    define MRDOCS_DETAIL_UNPAREN_I(...) __VA_ARGS__
#    define MRDOCS_DETAIL_UNPAREN(x)     MRDOCS_DETAIL_UNPAREN_I x

/** Evaluate an expected-like expression into a structured binding.

    Works like the two-argument form of `MRDOCS_TRY`, but unpacks the
    value into a structured binding. The expression is evaluated once.
    If it failed, the enclosing function returns `Unexpected` with its
    error. Otherwise the value is moved into `auto [names...]`. The
    enclosing function must return an @ref mrdocs::Expected. See `MRDOCS_TRY`
    for what counts as failed.

    Wrap the binding names in parentheses, so the commas between them
    aren't taken as macro-argument separators:

    @code
    Expected<std::pair<std::string, int>>
    splitKeyValue(std::string_view line);

    Expected<Option>
    parseOption(std::string_view line)
    {
        MRDOCS_TRY_BIND((key, value), splitKeyValue(line));
        return Option{std::move(key), value};
    }
    @endcode

    The body of `parseOption` expands to roughly this:

    @code
    // Simplified: the hidden local is really named
    // expected_result_<line>, and failed/error are
    // ::mrdocs::detail::failed and ::mrdocs::detail::error.

    // MRDOCS_TRY_BIND((key, value), splitKeyValue(line));
    auto tmp = splitKeyValue(line);
    if (failed(tmp)) {
        return Unexpected(error(tmp));
    }
    auto [key, value] = *std::move(tmp);

    return Option{std::move(key), value};
    @endcode

    Any type that supports structured bindings works, with as many names
    as it has elements:

    @code
    Expected<std::tuple<std::string, int, int>>
    parseLocation(std::string_view text);

    Expected<int>
    lineOf(std::string_view text)
    {
        MRDOCS_TRY_BIND((file, line, column), parseLocation(text));
        return line;
    }
    @endcode

    Things to keep in mind:

    @li The binding is always `auto [...]`: you can't make it `const` or
        a reference. The names bind to a fresh object moved out of the
        result.
    @li There's no custom-message form. If you need one, use
        `MRDOCS_TRY(auto v, expr, msg)` and bind `v` yourself.
    @li Like `MRDOCS_TRY`, it expands to several statements, needs a
        trailing semicolon, can't appear twice on the same line, and
        needs braces when it's the body of an `if`, `else`, or loop.

    @param names The parenthesized list of binding names.
    @param expr The expected-like expression to evaluate.
*/
#    define MRDOCS_TRY_BIND(names, expr)                   \
        auto MRDOCS_DETAIL_UNIQUE_NAME = expr;             \
        if (::mrdocs::detail::failed(MRDOCS_DETAIL_UNIQUE_NAME)) {   \
            return Unexpected(::mrdocs::detail::error(MRDOCS_DETAIL_UNIQUE_NAME)); \
        }                                                  \
        auto [MRDOCS_DETAIL_UNPAREN(names)] = *std::move(MRDOCS_DETAIL_UNIQUE_NAME)

#    define MRDOCS_DETAIL_CHECK_VOID(var)                  \
        if (::mrdocs::detail::failed(var)) {                         \
            return Unexpected(::mrdocs::detail::error(var));         \
        }                                                  \
        void(0)
#    define MRDOCS_DETAIL_CHECK_MSG(var, msg)              \
        if (::mrdocs::detail::failed(var)) {                         \
            return Unexpected(Error(msg));                 \
        }                                                  \
        void(0)
#    define MRDOCS_DETAIL_CHECK_SELECT(_1, _2, NAME, ...) NAME

/** Check an existing expected-like value and propagate its error.

    Tests a value you already have and, if it failed, makes the enclosing
    function return `Unexpected`. Unlike `MRDOCS_TRY`, the value isn't
    moved from or bound to a name. The enclosing function must return an
    @ref mrdocs::Expected (or anything constructible from `Unexpected`).

    A value counts as failed when it is an @ref mrdocs::Expected without a
    value, a failed @ref mrdocs::Error, an empty container or string, or
    anything that converts to `false` (pointers, `std::optional`,
    conditions). See `MRDOCS_TRY` for the details.

    The macro takes one or two arguments. The equivalent code below is
    simplified: `failed` and `error` are `::mrdocs::detail::failed` and
    `::mrdocs::detail::error`.

    With one argument, it forwards the error the value already holds:

    @code
    Expected<int> p = parsePort(port);
    MRDOCS_CHECK(p);
    @endcode

    is equivalent to:

    @code
    Expected<int> p = parsePort(port);
    if (failed(p)) {                  // !p, since p is an Expected
        return Unexpected(error(p));  // p.error()
    }
    @endcode

    A failed @ref mrdocs::Expected or @ref mrdocs::Error is forwarded as
    is, a failed container becomes the error `"Empty value"`, and a
    value that converts to `false` becomes `"Invalid value"`.

    With two arguments, it returns a new error built from the second
    argument instead:

    @code
    MRDOCS_CHECK(!host.empty(), "empty host");
    @endcode

    is equivalent to:

    @code
    if (failed(!host.empty())) {  // the condition is false
        return Unexpected(Error("empty host"));
    }
    @endcode

    The second argument can be anything @ref mrdocs::Error is
    constructible from, such as a non-empty string, a `std::error_code`,
    or another @ref mrdocs::Error. It's only evaluated when the check
    fails, so building a message there is cheap on success:

    @code
    MRDOCS_CHECK(*p < 65536, formatError("port {} out of range", *p));
    @endcode

    Pass a message when `"Empty value"` or `"Invalid value"` wouldn't
    help the caller.

    Things to keep in mind:

    @li Use it only inside a function body, and end it with a semicolon.
    @li In the one-argument form, a failed argument is evaluated twice,
        once to test it and once to read its error, so pass a named
        value rather than a call like `MRDOCS_CHECK(parse(s))`. Use
        `MRDOCS_TRY` for that. The two-argument form evaluates it once.
    @li The expansion is two statements. Put braces around it when it's
        the body of an `if`, `else`, or loop.
    @li Wrap an argument in parentheses if it has a top-level comma, such
        as a template argument list.
    @li The two-argument form names `Error` without qualification.
        Outside namespace `mrdocs`, bring it in with
        `using mrdocs::Error;`.

    To return early from a function that doesn't return an
    @ref mrdocs::Expected, use `MRDOCS_CHECK_OR`.
*/
#    define MRDOCS_CHECK(...) \
        MRDOCS_DETAIL_CHECK_SELECT(__VA_ARGS__, MRDOCS_DETAIL_CHECK_MSG, MRDOCS_DETAIL_CHECK_VOID)(__VA_ARGS__)

#    define MRDOCS_DETAIL_CHECK_OR_VOID(var)               \
        if (::mrdocs::detail::failed(var)) {                         \
            return;                                        \
        }                                                  \
        void(0)
#    define MRDOCS_DETAIL_CHECK_OR_VALUE(var, value)       \
        if (::mrdocs::detail::failed(var)) {                         \
            return value;                                  \
        }                                                  \
        void(0)
#    define MRDOCS_DETAIL_CHECK_OR_SELECT(_1, _2, NAME, ...) NAME

/** Check a value and return early if it failed.

    Use this in functions that don't return an @ref mrdocs::Expected. If the
    value failed, the enclosing function returns the second argument,
    or just returns when there's no second argument. No error is
    produced. See `MRDOCS_TRY` for what counts as failed.

    The macro takes one or two arguments. The equivalent code below is
    simplified: `failed` is `::mrdocs::detail::failed`. The examples use
    this type:

    @code
    struct Node
    {
        Node* parent = nullptr;
        std::string name;
        int n = 0;
    };
    @endcode

    With one argument, it does a plain `return;`, so it only works in a
    function that returns `void`:

    @code
    void
    rename(Node* node, std::string name)
    {
        MRDOCS_CHECK_OR(node);
        node->name = std::move(name);
    }
    @endcode

    is equivalent to:

    @code
    void
    rename(Node* node, std::string name)
    {
        if (failed(node)) {  // !node, since it's a pointer
            return;
        }
        node->name = std::move(name);
    }
    @endcode

    With two arguments, it returns the second argument instead:

    @code
    Node const*
    grandparent(Node const& node)
    {
        MRDOCS_CHECK_OR(node.parent, nullptr);
        return node.parent->parent;
    }
    @endcode

    is equivalent to:

    @code
    Node const*
    grandparent(Node const& node)
    {
        if (failed(node.parent)) {
            return nullptr;
        }
        return node.parent->parent;
    }
    @endcode

    The second argument can be anything convertible to the function's
    return type, including a braced `{}` for a default value:

    @code
    std::optional<int>
    count(Node const* node)
    {
        MRDOCS_CHECK_OR(node, std::nullopt);
        MRDOCS_CHECK_OR(node->n > 0, {});  // returns an empty optional
        return node->n;
    }
    @endcode

    The checked value is evaluated once, so a call expression is fine
    here. The return value is only evaluated when the check fails.

    Things to keep in mind:

    @li Use it only inside a function body, and end it with a semicolon.
    @li The one-argument form only works in a function that returns
        `void`. The two-argument form needs a value convertible to the
        function's return type.
    @li The expansion is two statements. Put braces around it when it's
        the body of an `if`, `else`, or loop.
    @li Wrap an argument in parentheses if it has a top-level comma, such
        as `(std::pair<int, int>{})`.
*/
#    define MRDOCS_CHECK_OR(...) \
        MRDOCS_DETAIL_CHECK_OR_SELECT(__VA_ARGS__, MRDOCS_DETAIL_CHECK_OR_VALUE, MRDOCS_DETAIL_CHECK_OR_VOID)(__VA_ARGS__)

/** Check a value and skip to the next loop iteration if it failed.

    If the value failed, the macro runs `continue`. See `MRDOCS_TRY`
    for what counts as failed.

    @code
    // Node is the struct from the MRDOCS_CHECK_OR example.
    int total = 0;
    for (Node const* node : nodes)
    {
        MRDOCS_CHECK_OR_CONTINUE(node);         // skip null pointers
        MRDOCS_CHECK_OR_CONTINUE(node->name);   // skip empty names
        MRDOCS_CHECK_OR_CONTINUE(node->n > 0);  // skip non-positive counts
        total += node->n;
    }
    @endcode

    The loop expands to roughly this (simplified: the real code calls
    `::mrdocs::detail::failed`):

    @code
    int total = 0;
    for (Node const* node : nodes)
    {
        // MRDOCS_CHECK_OR_CONTINUE(node);
        if (failed(node)) {         // !node, since node is a pointer
            continue;
        }

        // MRDOCS_CHECK_OR_CONTINUE(node->name);
        if (failed(node->name)) {   // node->name.empty(), a string
            continue;
        }

        // MRDOCS_CHECK_OR_CONTINUE(node->n > 0);
        if (failed(node->n > 0)) {  // the condition is false
            continue;
        }

        total += node->n;
    }
    @endcode

    The value is evaluated once, so a call expression like
    `MRDOCS_CHECK_OR_CONTINUE(isVisible(node))` is fine.

    Things to keep in mind:

    @li Use it only inside a loop, and end it with a semicolon.
    @li The expansion is two statements. Put braces around it when it's
        the body of an `if` or `else`.

    @param var The value to check.
*/
#    define MRDOCS_CHECK_OR_CONTINUE(var)                  \
        if (::mrdocs::detail::failed(var)) {                         \
            continue;                                      \
        }                                                  \
        void(0)

/** Check a value and leave the enclosing loop if it failed.

    If the value failed, the macro runs `break`. See `MRDOCS_TRY` for
    what counts as failed.

    @code
    // Count the leading results that succeeded.
    std::size_t n = 0;
    for (Expected<int> const& r : results)
    {
        MRDOCS_CHECK_OR_BREAK(r);   // stop at the first error
        ++n;
    }
    @endcode

    The loop expands to roughly this (simplified: the real code calls
    `::mrdocs::detail::failed`):

    @code
    std::size_t n = 0;
    for (Expected<int> const& r : results)
    {
        // MRDOCS_CHECK_OR_BREAK(r);
        if (failed(r)) {   // !r, since r is an Expected
            break;
        }

        ++n;
    }
    @endcode

    The value is evaluated once, so a call expression is fine.

    Things to keep in mind:

    @li Use it only inside a loop or `switch`, and end it with a
        semicolon.
    @li It's a plain `break`, so inside a `switch` it leaves the
        `switch`, not the loop around it.
    @li The expansion is two statements. Put braces around it when it's
        the body of an `if` or `else`.

    @param var The value to check.
*/
#    define MRDOCS_CHECK_OR_BREAK(var)     \
        if (::mrdocs::detail::failed(var)) \
        {                                  \
            break;                         \
        }                                  \
        void(0)

#endif

} // namespace mrdocs

#endif
