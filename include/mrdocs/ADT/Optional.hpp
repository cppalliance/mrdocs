//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ADT_OPTIONAL_HPP
#define MRDOCS_API_ADT_OPTIONAL_HPP

#include <mrdocs/Platform.hpp>
#include <concepts>
#include <type_traits>
#include <utility>
#include <ranges>

namespace clang::mrdocs {

/** The default empty predicate.

    This predicate is true when t.empty() returns
    `true` where `t` is a `T`.
*/
struct DefaultEmptyPredicate
{
    template <std::ranges::range T>
    constexpr
    bool
    operator()(T const& t) const noexcept
    {
        return std::ranges::empty(t);
    }

    template<class T>
    constexpr
    bool
    operator()(T const& t) const noexcept
    {
        return !t;
    }


};

/** A compact optional.

    This works like std::optional except the
    predicate is invoked to determine whether
    the optional is engaged. This is a space
    optimization.
*/
template<
    class T,
    class EmptyPredicate = DefaultEmptyPredicate>
class Optional
{
    T t_;

public:
    using value_type = T;

    constexpr Optional() = default;
    constexpr Optional(Optional const& other) = default;
    constexpr Optional& operator=(Optional const& other) = default;
    constexpr Optional& operator=(Optional&& other) = default;

    template<class U>
    requires std::is_constructible_v<T, U>
    constexpr explicit
    Optional(U&& u)
        : t_(std::forward<U>(u))
    {
    }

    constexpr void reset()
    {
        *this = Optional();
    }

    template<typename... Args>
    requires std::is_constructible_v<T, Args...>
    constexpr value_type& emplace(Args&&... args)
    {
        return t_ = T(std::forward<Args>(args)...);
    }

    constexpr value_type& value() & noexcept
    {
        return t_;
    }

    constexpr value_type const& value() const & noexcept
    {
        return t_;
    }

    constexpr value_type&& value() && noexcept
    {
        return std::move(t_);
    }

    constexpr value_type const&& value() const && noexcept
    {
        return std::move(t_);
    }

    constexpr value_type& operator*() noexcept
    {
        return t_;
    }

    constexpr value_type const& operator*() const noexcept
    {
        return t_;
    }

    constexpr value_type* operator->() noexcept
    {
        return &t_;
    }

    constexpr value_type const* operator->() const noexcept
    {
        return &t_;
    }

    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    constexpr bool has_value() const noexcept
    {
        return ! EmptyPredicate()(t_);
    }

    auto operator<=>(Optional const&) const = default;
};

} // clang::mrdocs

#endif
