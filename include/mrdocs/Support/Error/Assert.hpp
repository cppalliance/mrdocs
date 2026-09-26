//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ERROR_ASSERT_HPP
#define MRDOCS_API_SUPPORT_ERROR_ASSERT_HPP

#include <cstdint>

/** Core MrDocs support utilities.

    The public `mrdocs` namespace here centralizes assert/assume hooks so we can
    swap behavior (terminate, throw, debugbreak) in one place without leaking
    platform specifics into the rest of the codebase.
*/
namespace mrdocs {

#ifdef NDEBUG
    #ifdef __GNUC__
        #define MRDOCS_DETAIL_UNREACHABLE() static_cast<void>(__builtin_unreachable())
    #elif defined(_MSC_VER)
        #define MRDOCS_DETAIL_UNREACHABLE() static_cast<void>(__assume(false))
    #endif
    #define MRDOCS_DETAIL_ASSERT(x) static_cast<void>(false)
#else
    #ifdef __GNUC__
        #define MRDOCS_DETAIL_UNREACHABLE() static_cast<void>(__builtin_trap(), __builtin_unreachable())
    #elif defined(_MSC_VER)
        #define MRDOCS_DETAIL_UNREACHABLE() static_cast<void>(__debugbreak(), __assume(false))
    #endif

    /** Handler invoked when `MRDOCS_ASSERT` fails.

        @param msg Expression string that failed.
        @param file Source file where the assertion triggered.
        @param line Line within the source file.
    */
    void
    assert_failed(
        char const* msg,
        char const* file,
        std::uint_least32_t line);

    #define MRDOCS_DETAIL_ASSERT(x) static_cast<void>(!! (x) || \
        (assert_failed(#x, __builtin_FILE(), __builtin_LINE()), \
        MRDOCS_DETAIL_UNREACHABLE(), true))
#endif

/** Mark a code path that can never run.

    Use it as a statement, with a trailing semicolon, after code that
    handles every possible case, such as a `switch` over all
    enumerators. It also stops "control reaches end of non-void
    function" warnings, since the compiler knows the path doesn't
    return:

    @code
    namespace mrdocs {

    enum class Access { Public, Protected, Private };

    char const*
    toString(Access a)
    {
        switch (a)
        {
        case Access::Public:    return "public";
        case Access::Protected: return "protected";
        case Access::Private:   return "private";
        }
        MRDOCS_UNREACHABLE();
    }

    } // mrdocs
    @endcode

    The last line of `toString` expands to:

    @code
    // MRDOCS_UNREACHABLE();

    // Debug builds: trap at once, so a bug that reaches this
    // line stops the program where it happened. No message
    // is printed.
    static_cast<void>(__builtin_trap(), __builtin_unreachable());
    // MSVC: static_cast<void>(__debugbreak(), __assume(false));

    // Release builds (NDEBUG): only an optimizer hint.
    static_cast<void>(__builtin_unreachable());
    // MSVC: static_cast<void>(__assume(false));
    @endcode

    In release builds, actually reaching the call is undefined
    behavior, so don't use it for paths that bad input can reach.
    Return an error or throw there instead. The macro takes no
    arguments, but the empty parentheses are required. Unlike
    `MRDOCS_ASSERT`, it works in any namespace.

    When to use it:

    @li In the `default:` of a `switch` over a closed kind enum, when
        every enumerator is already handled, as in the `AccessKind`
        and `AttributeKind` switches in `ASTVisitor.cpp`.
    @li At the end of an exhaustive dispatch, such as the `asX()`
        casts generated from the `.inc` kind lists or the final `else`
        of the kind-id chain in `Visitor.hpp`.

    Use it when the location itself is the bug and there's no
    condition to test. When you can state the condition that must
    hold, such as `t->Kind == T::kind_id` before a cast, use
    @ref MRDOCS_ASSERT instead, so a debug build reports the failed
    expression. Neither macro is for input a user can control: return
    an error with `MRDOCS_CHECK` or `MRDOCS_CHECK_OR` there.

    @see @ref MRDOCS_ASSERT
*/
#define MRDOCS_UNREACHABLE() MRDOCS_DETAIL_UNREACHABLE()

/** Assert that a condition holds.

    Use it as a statement, with a trailing semicolon, to check a
    precondition or invariant:

    @code
    namespace mrdocs {

    int
    elementAt(std::vector<int> const& v, std::size_t i)
    {
        MRDOCS_ASSERT(i < v.size());
        return v[i];
    }

    } // mrdocs
    @endcode

    In debug builds, the assertion roughly expands to:

    @code
    // MRDOCS_ASSERT(i < v.size());
    // The condition is evaluated once. If it's false,
    // assert_failed prints "assertion failed: i < v.size()
    // on line N in <file>" to stderr, and then the program
    // traps (__builtin_trap on GCC/Clang, __debugbreak on
    // MSVC).
    static_cast<void>(!!(i < v.size()) ||
        (assert_failed("i < v.size()",
            __builtin_FILE(), __builtin_LINE()),
         static_cast<void>(__builtin_trap(),
             __builtin_unreachable()),
         true));
    @endcode

    In release builds (`NDEBUG`), it expands to:

    @code
    // MRDOCS_ASSERT(i < v.size());
    // The condition is dropped by the preprocessor. It's
    // never compiled or evaluated.
    static_cast<void>(false);
    @endcode

    Some consequences of this definition:

    - The condition must not have side effects the program
      depends on, such as `MRDOCS_ASSERT(queue.pop())`, since
      release builds never run it. Variables used only inside
      assertions can trigger unused-variable warnings there.
    - The expansion calls `assert_failed` unqualified, so in
      debug builds the macro only compiles where
      `mrdocs::assert_failed` is found by lookup: inside
      `namespace mrdocs`, or after `using namespace mrdocs;`.
    - It's a `void` expression, not a full statement, so a
      trailing semicolon is needed.
    - It's a single-argument macro. Wrap a condition that has a
      top-level comma in parentheses, as in
      `MRDOCS_ASSERT((std::is_same_v<A, B>))`.

    When to use it:

    @li Preconditions of a function, such as a non-null pointer from
        Clang (`MRDOCS_ASSERT(D)`) or an index in range.
    @li Invariants of a type, such as `has_value()` in an accessor,
        `!valueless_after_move()` on a `Polymorphic`, or
        `t->Kind == T::kind_id` before a cast to the derived type.
    @li Assumptions a later line depends on, such as a container
        being non-empty before `front()`.

    Use it when there's a condition you can state, so a debug build
    prints it when it fails. When there's no condition and the
    location itself is the bug, such as the `default:` of a `switch`
    that handles every enumerator, use @ref MRDOCS_UNREACHABLE
    instead. Neither macro is for input a user can control, since
    release builds don't check it: return an error with
    `MRDOCS_CHECK` or `MRDOCS_CHECK_OR` there.

    @param x The condition to check.

    @see @ref MRDOCS_UNREACHABLE
*/
#define MRDOCS_ASSERT(x) MRDOCS_DETAIL_ASSERT(x)

} // mrdocs

#endif
