//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ADT_OPTIONAL_HPP
#define MRDOCS_API_ADT_OPTIONAL_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Nullable.hpp>
#include <compare>
#include <optional>
#include <type_traits>
#include <utility>

namespace clang::mrdocs {

/** A compact optional that automatically uses nullable_traits<T> when
    available.

    Design
    - If nullable_traits<T> exists, the null state is encoded inside T
      (via sentinel or clearable-empty semantics). Storage is exactly one T.
    - Otherwise, this falls back to std::optional<T> and uses its discriminator.

    This single implementation uses a conditional storage type plus if constexpr
    on has_nullable_traits_v<T> to select the appropriate behavior at compile
    time.
**/
template <class T>
class Optional {
    using storage_t = std::conditional_t<
        has_nullable_traits_v<T>, T, std::optional<T>>;

    static constexpr bool uses_nullable_traits = has_nullable_traits_v<T>;

    // --- noexcept helpers (avoid instantiating both branches) ---
    static consteval bool default_ctor_noex_()
    {
        if constexpr (uses_nullable_traits)
            return noexcept(nullable_traits<T>::null());
        else
            return noexcept(storage_t{}); // std::optional<T>{} is noexcept
    }

    static consteval bool reset_noex_()
    {
        if constexpr (uses_nullable_traits)
            return noexcept(nullable_traits<T>::make_null(std::declval<T&>()));
        else
            return noexcept(std::declval<std::optional<T>&>().reset());
    }

    static consteval bool has_value_noex_()
    {
        if constexpr (uses_nullable_traits)
            return noexcept(nullable_traits<T>::is_null(std::declval<T const&>()));
        else
            return noexcept(std::declval<std::optional<T> const&>().has_value());
    }

    storage_t s_;

public:
    using value_type = T;

    /// Default-constructs to the “null” state.
    constexpr Optional() noexcept(default_ctor_noex_())
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                return storage_t(nullable_traits<T>::null());
            } else
            {
                return storage_t(std::nullopt);
            }
        }())
    {}

    /** Construct from std::nullopt
     */
    constexpr Optional(std::nullopt_t) noexcept(default_ctor_noex_())
        : Optional() {}

    /// Copy constructor
    constexpr Optional(Optional const&) = default;

    /// Move constructor
    constexpr Optional(Optional&&) = default;

    /// Copy assignment
    constexpr Optional&
    operator=(Optional const&) = default;

    /// Move assignment
    constexpr Optional&
    operator=(Optional&&) = default;

    /** Construct from a value.

        @param u The value to store. It must be convertible to T.
     **/
    template <class U>
    requires(
        !std::same_as<std::remove_cvref_t<U>, Optional>
        && !std::same_as<std::remove_cvref_t<U>, std::in_place_t>
        && !std::same_as<std::remove_cvref_t<U>, std::nullopt_t>
        && std::is_constructible_v<T, U>)
    constexpr explicit Optional(U&& u) noexcept(
        std::is_nothrow_constructible_v<T, U>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                return storage_t(static_cast<T>(std::forward<U>(u)));
            } else
            {
                return storage_t(std::forward<U>(u));
            }
        }())
    {}

    /** Assign from a value.

        @param u The value to store. It must be convertible to T.
     **/
    template <class U>
    requires(
        !std::same_as<std::remove_cvref_t<U>, Optional>
        && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>)
    constexpr
    Optional&
    operator=(U&& u) noexcept(std::is_nothrow_assignable_v<T&, U>)
    {
        if constexpr (uses_nullable_traits)
        {
            s_ = std::forward<U>(u);
        } else
        {
            s_ = std::forward<U>(u);
        }
        return *this;
    }

    /// Assign null (disengage).
    constexpr Optional&
    operator=(std::nullptr_t) noexcept(reset_noex_())
    {
        reset();
        MRDOCS_ASSERT(!has_value());
        return *this;
    }

    /** Reset to the null state. **/
    constexpr void
    reset() noexcept(reset_noex_())
    {
        if constexpr (uses_nullable_traits)
        {
            nullable_traits<T>::make_null(s_);
        }
        else
        {
            s_.reset();
        }
        MRDOCS_ASSERT(!has_value());
    }

    /** In-place construct a new value, replacing any existing one.

        @param args The arguments to forward to T's constructor.

        @return A reference to the newly constructed value.
     **/
    template <class... Args>
    requires std::is_constructible_v<T, Args...>
    constexpr value_type&
    emplace(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
    {
        if constexpr (uses_nullable_traits)
        {
            s_ = T(std::forward<Args>(args)...);
            return s_;
        }
        else
        {
            return s_.emplace(std::forward<Args>(args)...);
        }
    }

    /** True if engaged (contains a value).

        @return `true` if the optional contains a value.
     **/
    constexpr bool
    has_value() const noexcept(has_value_noex_())
    {
        if constexpr (uses_nullable_traits)
        {
            return !nullable_traits<T>::is_null(s_);
        }
        else
        {
            return s_.has_value();
        }
    }

    /** Contextual bool.
     **/
    constexpr explicit
    operator bool() const noexcept(noexcept(this->has_value()))
    {
        return has_value();
    }

    /** Value access. Preconditions: has_value() is true.

        @return A reference to the contained value.
     **/
    constexpr value_type&
    value() & noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return s_;
        }
        else
        {
            return *s_;
        }
    }

    /// @copydoc value() &
    constexpr value_type const&
    value() const& noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return s_;
        }
        else
        {
            return *s_;
        }
    }

    /// @copydoc value() &
    constexpr value_type&&
    value() && noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return std::move(s_);
        }
        else
        {
            return std::move(*s_);
        }
    }

    /// @copydoc value() &
    constexpr value_type const&&
    value() const&& noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return std::move(s_);
        }
        else
        {
            return std::move(*s_);
        }
    }

    /** Pointer-like access.

        @return A pointer to the contained value.
     **/
    constexpr value_type*
    operator->() noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return std::addressof(s_);
        }
        else
        {
            return std::addressof(*s_);
        }
    }

    /// @copydoc operator->() noexcept
    constexpr value_type const*
    operator->() const noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return std::addressof(s_);
        }
        else
        {
            return std::addressof(*s_);
        }
    }

    /** Dereference-like access.

        @return A reference to the contained value.
     **/
    constexpr value_type&
    operator*() noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return s_;
        }
        else
        {
            return *s_;
        }
    }

    /// @copydoc operator*() noexcept
    constexpr value_type const&
    operator*() const noexcept
    {
        MRDOCS_ASSERT(has_value());
        if constexpr (uses_nullable_traits)
        {
            return s_;
        }
        else
        {
            return *s_;
        }
    }
};

namespace detail {
template<typename T>
inline constexpr bool isOptionalV = false;

template<typename T>
inline constexpr bool isOptionalV<Optional<T>> = true;

template <typename T>
using OptionalRelopT = std::enable_if_t<std::is_convertible_v<T, bool>, bool>;

template <typename T, typename U>
using OptionalEqT = OptionalRelopT<
    decltype(std::declval<T const&>() == std::declval<U const&>())>;

template <typename T, typename U>
using OptionalNeT = OptionalRelopT<
    decltype(std::declval<T const&>() != std::declval<U const&>())>;

template <typename T, typename U>
using OptionalLtT = OptionalRelopT<
    decltype(std::declval<T const&>() < std::declval<U const&>())>;

template <typename T, typename U>
using OptionalGtT = OptionalRelopT<
    decltype(std::declval<T const&>() > std::declval<U const&>())>;

template <typename T, typename U>
using OptionalLeT = OptionalRelopT<
    decltype(std::declval<T const&>() <= std::declval<U const&>())>;

template <typename T, typename U>
using OptionalGeT = detail::OptionalRelopT<
    decltype(std::declval<T const&>() >= std::declval<U const&>())>;

template <typename T>
concept isDerivedFromOptional = requires(T const& __t) {
    []<typename U>(Optional<U> const&) {
    }(__t);
};

} // namespace detail

/** Compares two Optional values for equality.

    Returns true if both are engaged and their contained values are equal, or both are disengaged.
*/
template <typename T, typename U>
constexpr detail::OptionalEqT<T, U>
operator==(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return static_cast<bool>(lhs) == static_cast<bool>(rhs)
           && (!lhs || *lhs == *rhs);
}

/** Compares two Optional values for inequality.

    Returns true if their engagement states differ or their contained values are not equal.
*/
template <typename T, typename U>
constexpr detail::OptionalNeT<T, U>
operator!=(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return static_cast<bool>(lhs) != static_cast<bool>(rhs)
           || (static_cast<bool>(lhs) && *lhs != *rhs);
}

/** Checks if the left Optional is less than the right Optional.

    Returns true if the right is engaged and either the left is disengaged or its value is less.
*/
template <typename T, typename U>
constexpr detail::OptionalLtT<T, U>
operator<(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return static_cast<bool>(rhs) && (!lhs || *lhs < *rhs);
}

/** Checks if the left Optional is greater than the right Optional.

    Returns true if the left is engaged and either the right is disengaged or its value is greater.
*/
template <typename T, typename U>
constexpr detail::OptionalGtT<T, U>
operator>(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return static_cast<bool>(lhs) && (!rhs || *lhs > *rhs);
}

/** Checks if the left Optional is less than or equal to the right Optional.

    Returns true if the left is disengaged or the right is engaged and the left's value is less or equal.
*/
template <typename T, typename U>
constexpr detail::OptionalLeT<T, U>
operator<=(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return !lhs || (static_cast<bool>(rhs) && *lhs <= *rhs);
}

/** Checks if the left Optional is greater than or equal to the right Optional.

    Returns true if the right is disengaged or the left is engaged and its value is greater or equal.
*/
template <typename T, typename U>
constexpr detail::OptionalGeT<T, U>
operator>=(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return !rhs || (static_cast<bool>(lhs) && *lhs >= *rhs);
}

/** Performs a three-way comparison between two Optional values.

    If both are engaged, compares their contained values; otherwise, compares engagement state.
*/
template <typename T, std::three_way_comparable_with<T> U>
[[nodiscard]]
constexpr std::compare_three_way_result_t<T, U>
operator<=>(Optional<T> const& x, Optional<U> const& y)
{
    return x && y ? *x <=> *y : bool(x) <=> bool(y);
}

/** Checks if the Optional is disengaged (equal to std::nullopt).

    Returns true if the Optional does not contain a value.
*/
template <typename T>
[[nodiscard]]
constexpr bool
operator==(Optional<T> const& lhs, std::nullopt_t) noexcept
{
    return !lhs;
}

/** Performs a three-way comparison between an Optional and std::nullopt.

    Returns std::strong_ordering::greater if engaged, std::strong_ordering::equal if disengaged.
*/
template <typename T>
[[nodiscard]]
constexpr std::strong_ordering
operator<=>(Optional<T> const& x, std::nullopt_t) noexcept
{
    return bool(x) <=> false;
}

/** Compares an engaged Optional to a value for equality.

    Returns true if the Optional is engaged and its value equals rhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalEqT<T, U>
operator== [[nodiscard]] (Optional<T> const& lhs, U const& rhs)
{
    return lhs && *lhs == rhs;
}

/** Compares a value to an engaged Optional for equality.

    Returns true if the Optional is engaged and its value equals lhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalEqT<T, U>
operator== [[nodiscard]] (T const& lhs, Optional<U> const& rhs)
{
    return rhs && lhs == *rhs;
}

/** Compares an Optional to a value for inequality.

    Returns true if the Optional is disengaged or its value does not equal rhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalNeT<T, U>
operator!= [[nodiscard]] (Optional<T> const& lhs, U const& rhs)
{
    return !lhs || *lhs != rhs;
}

/** Compares a value to an Optional for inequality.

    Returns true if the Optional is disengaged or its value does not equal lhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalNeT<T, U>
operator!= [[nodiscard]] (T const& lhs, Optional<U> const& rhs)
{
    return !rhs || lhs != *rhs;
}

/** Checks if the Optional is less than a value.

    Returns true if the Optional is disengaged or its value is less than rhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalLtT<T, U>
operator<[[nodiscard]] (Optional<T> const& lhs, U const& rhs)
{
    return !lhs || *lhs < rhs;
}

/** Checks if a value is less than an engaged Optional.

    Returns true if the Optional is engaged and lhs is less than its value.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalLtT<T, U>
operator<[[nodiscard]] (T const& lhs, Optional<U> const& rhs)
{
    return rhs && lhs < *rhs;
}

/** Checks if the Optional is greater than a value.

    Returns true if the Optional is engaged and its value is greater than rhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalGtT<T, U>
operator> [[nodiscard]] (Optional<T> const& lhs, U const& rhs)
{
    return lhs && *lhs > rhs;
}

/** Checks if a value is greater than an Optional.

    Returns true if the Optional is disengaged or lhs is greater than its value.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalGtT<T, U>
operator> [[nodiscard]] (T const& lhs, Optional<U> const& rhs)
{
    return !rhs || lhs > *rhs;
}

/** Checks if the Optional is less than or equal to a value.

    Returns true if the Optional is disengaged or its value is less than or equal to rhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalLeT<T, U>
operator<= [[nodiscard]] (Optional<T> const& lhs, U const& rhs)
{
    return !lhs || *lhs <= rhs;
}

/** Checks if a value is less than or equal to an engaged Optional.

    Returns true if the Optional is engaged and lhs is less than or equal to its value.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalLeT<T, U>
operator<= [[nodiscard]] (T const& lhs, Optional<U> const& rhs)
{
    return rhs && lhs <= *rhs;
}

/** Checks if the Optional is greater than or equal to a value.

    Returns true if the Optional is engaged and its value is greater than or equal to rhs.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalGeT<T, U>
operator>= [[nodiscard]] (Optional<T> const& lhs, U const& rhs)
{
    return lhs && *lhs >= rhs;
}

/** Checks if a value is greater than or equal to an Optional.

    Returns true if the Optional is disengaged or lhs is greater than or equal to its value.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalGeT<T, U>
operator>= [[nodiscard]] (T const& lhs, Optional<U> const& rhs)
{
    return !rhs || lhs >= *rhs;
}

/** Performs a three-way comparison between an Optional and a value.

    If the Optional is engaged, compares its value to v; otherwise, returns less.
*/
template <typename T, typename U>
requires(!detail::isDerivedFromOptional<U>)
        && requires { typename std::compare_three_way_result_t<T, U>; }
        && std::three_way_comparable_with<T, U>
constexpr std::compare_three_way_result_t<T, U>
operator<=> [[nodiscard]] (Optional<T> const& x, U const& v)
{
    return bool(x) ? *x <=> v : std::strong_ordering::less;
}
} // namespace clang::mrdocs

#endif
