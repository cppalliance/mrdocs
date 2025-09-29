//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ADT_OVERLOAD_HPP
#define MRDOCS_API_ADT_OVERLOAD_HPP

#include <mrdocs/Platform.hpp>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace mrdocs {

/** Combines multiple callable types into a single overloaded function object.

    This is the canonical "overloaded pattern" implemented as a class template.
    It inherits from all provided callables and brings in their operator()s,
    so the resulting object can be called with whichever overload matches.

    Typical use-cases include visiting std::variant and building small ad-hoc
    pattern-matching style dispatchers.

    @tparam Ts The callable types to combine (lambdas, function objects, etc.)

    @example
    @code
    auto f = fn::makeOverload(
     [](int i) { return i * 2; },
     [](const std::string& s) { return s.size(); }
    );
    auto a = f(21);        // calls int overload
    auto b = f(std::string("hello")); // calls string overload
    @endcode
 */
template<class... Ts>
struct Overload : Ts... {
    using Ts::operator()...;

    /** Constructs an Overload from the given callables.

        @param xs The callables to store.
     */
    constexpr explicit Overload(Ts... xs)
        noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
        : Ts(std::move(xs))... {}
};

/** Class template argument deduction guide for Overload.

    Allows writing Overload{lambda1, lambda2, ...} without
    specifying template parameters.
 */
template<class... Ts>
Overload(Ts...) -> Overload<Ts...>;

/** Factory function that creates an Overload from the given callables.

    Prefer this over constructing Overload directly when you need perfect
    forwarding and decayed storage.

    @param xs The callables to combine.
    @return An Overload whose base classes are the decayed types of the provided callables.

    @example
    @code
    auto visitor = fn::makeOverload(
         [](int) { return 1; },
         [](double) { return 2; }
    );
    @endcode

 */
template<class... Ts>
[[nodiscard]] constexpr auto makeOverload(Ts&&... xs)
    noexcept((std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts&&> && ...))
        -> Overload<std::decay_t<Ts>...>
{
    return Overload<std::decay_t<Ts>...>(std::forward<Ts>(xs)...);
}

/** Applies a set of callables to a std::variant using std::visit and Overload.

    This is a convenience wrapper around std::visit(makeOverload(...), variant).
    It forwards the variant and the callables and returns whatever std::visit
    returns.

    @param v The variant to visit (can be lvalue or rvalue; const-qualification is preserved).
    @param xs The callables to be combined with makeOverload and passed to std::visit.
    @return The result of std::visit.

    @example
    @code
    std::variant<int, std::string> v = 42;
    auto r = fn::match(v,
        [](int i) { return i + 1; },
        [](const std::string& s) { return s.size(); }
    );
    @endcode
 */
template<class Variant, class... Ts>
constexpr decltype(auto) match(Variant&& v, Ts&&... xs)
{
    return std::visit(
        makeOverload(std::forward<Ts>(xs)...),
        std::forward<Variant>(v));
}

/** Visits a std::variant and calls the combined callable with the active
    index and the value.

    Unlike match, visitIndexed passes an additional first parameter to your
    callable set: the runtime index of the active alternative (as a std::size_t).

    This is useful when you need both the value and which alternative was
    selected, without relying on type-unique alternatives.

    The supplied callables are combined via makeOverload and are expected to
    accept a signature like (std::size_t index, T value) for the relevant T.

    @param v The variant to visit.
    @param xs The callables to be combined with makeOverload and invoked with (index, value).
    @return The result of invoking the selected callable.

    @example
    @code
    std::variant<int, double, std::string> v = 3.14;
    fn::visitIndexed(v,
        [](std::size_t i, int x) { return i + x; },
        [](std::size_t i, double d) { return d + i; },
        [](std::size_t i, const std::string& s) { return s.size() + i; });
    @endcode
 */
template<class Variant, class... Ts>
constexpr decltype(auto) visitIndexed(Variant&& v, Ts&&... xs)
{
    auto f = makeOverload(std::forward<Ts>(xs)...);
    const std::size_t idx = std::as_const(v).index();
    return std::visit(
        [&](auto&& val) -> decltype(auto) {
        return f(idx, std::forward<decltype(val)>(val));
    },
        std::forward<Variant>(v)
    );
}

/** Enables recursive lambdas by passing a self-reference as the first argument.

    YCombinator stores a callable F and exposes operator() that forwards
    arguments to F, prepending a reference to *this so that F can recurse.

    Overloads are provided for &, const&, &&, const&& to preserve value
    category.

    @tparam F The callable to wrap.

    @example
    auto fact = fn::yCombinator(
        [](auto self, int n) -> long long {
            return n <= 1 ? 1 : n * self(n - 1);
        });
    auto r = fact(10);
    @endcode
 */
template<class F>
class YCombinator {
public:
    /** Constructs a YCombinator from the given callable.

        @param f The callable to store.
     */
    constexpr explicit YCombinator(F f)
        noexcept(std::is_nothrow_move_constructible_v<F>)
        : f_(std::move(f)) {}

    /** Invokes the stored callable, passing *this as the first parameter.

         @param args The arguments to forward to the callable after the self reference.
         @return Whatever the callable returns.
     */
    template<class... Args>
    constexpr decltype(auto) operator()(Args&&... args) &
    {
        return f_(*this, std::forward<Args>(args)...);
    }

    /** Const lvalue overload of operator().
     */
    template<class... Args>
    constexpr decltype(auto) operator()(Args&&... args) const &
    {
        return f_(*this, std::forward<Args>(args)...);
    }

    /** Rvalue overload of operator().
     */
    template<class... Args>
    constexpr decltype(auto) operator()(Args&&... args) &&
    {
        return std::move(f_)(*this, std::forward<Args>(args)...);
    }

    /** Const rvalue overload of operator().
     */
    template<class... Args>
    constexpr decltype(auto) operator()(Args&&... args) const &&
    {
        return std::move(f_)(*this, std::forward<Args>(args)...);
    }

private:
    F f_;
};

/** Factory that creates a YCombinator from a callable.

    Prefer this helper to avoid spelling template arguments explicitly.

    @param f The callable to wrap.
    @return A YCombinator storing a decayed copy of the callable.

    @example
    auto fib = fn::yCombinator(
        [](auto self, int n) -> int {
            return n <= 1 ? n : self(n - 1) + self(n - 2);
        });
 */
template<class F>
[[nodiscard]] constexpr auto yCombinator(F&& f)
    noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F&&>)
        -> YCombinator<std::decay_t<F>>
{
    return YCombinator<std::decay_t<F>>(std::forward<F>(f));
}

} // mrdocs

#endif // MRDOCS_API_ADT_OVERLOAD_HPP
