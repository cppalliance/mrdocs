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

#ifndef MRDOCS_API_ADT_NULLABLE_HPP
#define MRDOCS_API_ADT_NULLABLE_HPP

#include <mrdocs/Platform.hpp>

#include <type_traits>
#include <utility>
#include <memory>
#include <concepts>
#include <cmath>

namespace clang::mrdocs {

/** Defines a customization point for types that have an intrinsic sentinel
    value denoting “null”.

    Users may specialize this trait for their own types to declare a
    sentinel-based null representation.

    When enabled, nullable semantics can be implemented in terms of the
    sentinel without storing a separate engaged/disengaged flag.

    Contract for specializations:
    - Provide static constexpr T sentinel() noexcept; which returns the distinguished null value.
    - Provide static constexpr bool is_sentinel(const T&) noexcept; which recognizes the null value.

    If a type does not have a well-defined sentinel, leave the primary template in effect.

    Notes
    - Built-in pointer types and std::nullptr_t are pre-specialized to use nullptr as the sentinel.
**/
template <class T>
struct sentinel_traits
{
    // No sentinel() or is_sentinel() in the primary template.
};

/** sentinel_traits specialization for raw pointers.

    Uses nullptr as the sentinel value.
**/
template <class T>
struct sentinel_traits<T*> {
    static constexpr T*
    sentinel() noexcept
    {
        return nullptr;
    }

    static constexpr bool
    is_sentinel(T const* p) noexcept
    {
        return p == nullptr;
    }
};

/** sentinel_traits specialization for std::nullptr_t.
**/
template<>
struct sentinel_traits<std::nullptr_t>
{
    static constexpr std::nullptr_t
    sentinel() noexcept
    {
        return nullptr;
    }

    static constexpr bool
    is_sentinel(std::nullptr_t) noexcept
    {
        return true;
    }
};

// -----------------------------------------------------------------------------
// Sentinel traits for numeric and enum types
// -----------------------------------------------------------------------------

/** sentinel_traits specialization for unsigned integral types.

    Uses the maximum representable value (~0u) as the sentinel,
    which corresponds to -1 when converted.
**/
template <std::unsigned_integral T>
struct sentinel_traits<T>
{
    static constexpr T
    sentinel() noexcept
    {
        return static_cast<T>(-1);
    }

    static constexpr bool
    is_sentinel(T v) noexcept
    {
        return v == static_cast<T>(-1);
    }
};

/** sentinel_traits specialization for floating-point types.

    Uses a quiet NaN as the sentinel value. This assumes that T
    supports NaN and that it is distinguishable from all ordinary values.
**/
template <std::floating_point T>
struct sentinel_traits<T>
{
    static constexpr T
    sentinel() noexcept
    {
        return std::numeric_limits<T>::quiet_NaN();
    }

    static constexpr bool
    is_sentinel(T v) noexcept
    {
        return std::isnan(v);
    }
};

/** sentinel_traits specialization for enums with a well-known "null" enumerator.

    If the enum defines Unknown, UNKNOWN, None, or NONE, this trait uses
    that enumerator as the sentinel. This requires that such an enumerator
    exists and is accessible from the scope of T.
**/
template <typename T>
requires std::is_enum_v<T> &&
         (requires { T::Unknown; } ||
          requires { T::UNKNOWN; } ||
          requires { T::None; } ||
          requires { T::NONE; })
struct sentinel_traits<T>
{
    static constexpr T
    sentinel() noexcept
    {
        if constexpr (requires { T::Unknown; })
            return T::Unknown;
        else if constexpr (requires { T::UNKNOWN; })
            return T::UNKNOWN;
        else if constexpr (requires { T::None; })
            return T::None;
        else
            return T::NONE;
    }

    static constexpr bool
    is_sentinel(T v) noexcept
    {
        return v == sentinel();
    }
};

/** Concept that is satisfied when sentinel_traits<T> declares a usable sentinel.
**/
template<class T>
concept HasSentinel =
    requires
    {
        { sentinel_traits<T>::sentinel() } -> std::same_as<T>;
        { sentinel_traits<T>::is_sentinel(std::declval<const T&>()) } -> std::convertible_to<bool>;
    };

/** Internal concept that matches “empty-clear default-constructible” types.

    This captures the common case of containers and data structures that
    can be default-constructed to empty, tested with .empty(), and
    reset with .clear().

    Common cases of such containers include std::string, std::vector,
    std::optional, std::unique_ptr, std::shared_ptr, and many more.
**/
template<class T>
concept ClearableEmpty =
    std::default_initializable<T> &&
    requires(T& t, const T& ct)
    {
        { ct.empty() } -> std::convertible_to<bool>;
        { t.clear() } -> std::same_as<void>;
    };

/** nullable_traits<T> defines how to treat a T as “nullable” without an
   external engaged bit.

    This trait is the canonical place to encode nullability semantics used
    by any optional-like type.

    It exposes the minimal operations needed by an optional:
    - is_null(const T&): test if a value is null.
    - null(): create a null value.
    - make_null(T&): turn an existing value into null.

    Users may explicitly specialize nullable_traits for their types to define
    the desired semantics.
**/
template<class T>
struct nullable_traits
{
    // No interface in the primary template.
    // Provide a specialization to enable.
};

/** nullable_traits for types with a sentinel.

    Delegates null handling to sentinel_traits<T>.
**/
template<class T>
requires HasSentinel<T>
struct nullable_traits<T>
{
    static constexpr bool
    is_null(T const& v) noexcept
    {
        return sentinel_traits<T>::is_sentinel(v);
    }

    static constexpr T
    null() noexcept
    {
        return sentinel_traits<T>::sentinel();
    }

    static constexpr void
    make_null(T& v) noexcept
    {
        v = sentinel_traits<T>::sentinel();
    }
};

/** nullable_traits for clearable empty types.

    Treats the empty state as null, creates null via default construction,
    and erases via clear().
**/
template<class T>
requires (!HasSentinel<T> && ClearableEmpty<T>)
struct nullable_traits<T>
{
    static constexpr bool
    is_null(T const& v) noexcept(noexcept(v.empty()))
    {
        return v.empty();
    }

    static constexpr T
    null() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        return T();
    }

    static constexpr void
    make_null(T& v) noexcept(noexcept(v.clear()))
    {
        v.clear();
    }
};

/** Utility function that returns true if T has a nullable_traits
    specialization enabled.
**/
template<class T>
concept has_nullable_traits_v  =
    requires
    {
        { nullable_traits<T>::is_null(std::declval<const T&>()) } -> std::convertible_to<bool>;
        { nullable_traits<T>::null() } -> std::same_as<T>;
        { nullable_traits<T>::make_null(std::declval<T&>()) } -> std::same_as<void>;
    };

/** make_null helper that uses nullable_traits<T> if available.

    @param v The value to make null.
**/
template <has_nullable_traits_v T>
inline void
make_null(T& v) noexcept(noexcept(nullable_traits<T>::make_null(v)))
{
    nullable_traits<T>::make_null(v);
}

/** is_null helper that uses nullable_traits<T> if available.

    @param v The value to test for null.
    @return true if v is null, false otherwise.
**/
template <has_nullable_traits_v T>
inline bool
is_null(T const& v) noexcept(noexcept(nullable_traits<T>::is_null(v)))
{
    return nullable_traits<T>::is_null(v);
}

/** null_of helper that constructs a null T using nullable_traits<T>.

    @return A null T value.
**/
template <has_nullable_traits_v T>
inline T
null_of() noexcept(noexcept(nullable_traits<T>::null()))
{
    return nullable_traits<T>::null();
}

} // clang::mrdocs

#endif
