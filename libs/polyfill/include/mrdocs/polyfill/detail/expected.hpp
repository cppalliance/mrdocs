//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_POLYFILL_DETAIL_EXPECTED_HPP
#define MRDOCS_POLYFILL_DETAIL_EXPECTED_HPP

// Implementation details for <mrdocs/polyfill/expected.hpp>: the trait
// predicates, invocation-result aliases, and the reinit/guard helpers used
// by expected. Kept apart so expected.hpp reads as the interface.

#include <mrdocs/polyfill/type_traits.hpp>
#include <memory>
#include <type_traits>
#include <utility>

namespace mrdocs::polyfill {

template <class T, class E>
class expected;

template <class E>
class unexpected;

namespace detail
{
    template <class T>
    constexpr bool isExpected = false;
    template <class T, class E>
    constexpr bool isExpected<expected<T, E>> = true;

    template <class T>
    constexpr bool isUnexpected = false;
    template <class T>
    constexpr bool isUnexpected<unexpected<T>> = true;

    template <class Fn, class T>
    using ThenResult = std::remove_cvref_t<std::invoke_result_t<Fn&&, T&&>>;
    template <class Fn, class T>
    using ResultTransform = std::remove_cv_t<std::invoke_result_t<Fn&&, T&&>>;
    template <class Fn>
    using result0 = std::remove_cvref_t<std::invoke_result_t<Fn&&>>;
    template <class Fn>
    using result0Transform = std::remove_cv_t<std::invoke_result_t<Fn&&>>;

    template <class E>
    concept can_beUnexpected =
        std::is_object_v<E> &&
        (!std::is_array_v<E>) &&
        (!detail::isUnexpected<E>) &&
        (!std::is_const_v<E>) &&
        (!std::is_volatile_v<E>);

    // Tag types for in-place construction from an invocation result.
    struct in_place_inv { };
    struct unexpect_inv { };

    template <class R, class U>
    inline constexpr bool ok_bind_ref_v
        = std::is_constructible_v<R, U>
          && !reference_constructs_from_temporary_v<R, U>;
}

namespace detail
{
    template <class T>
    class ExpectedGuard
    {
        static_assert( std::is_nothrow_move_constructible_v<T> );
        T* guarded_;
        T tmp_;

    public:
        constexpr explicit
        ExpectedGuard(T& x)
            : guarded_(std::addressof(x))
            , tmp_(std::move(x))
        {
            std::destroy_at(guarded_);
        }

        constexpr
        ~ExpectedGuard()
        {
            if (guarded_) [[unlikely]]
            {
                std::construct_at(guarded_, std::move(tmp_));
            }
        }

        ExpectedGuard(ExpectedGuard const&) = delete;

        ExpectedGuard& operator=(ExpectedGuard const&) = delete;

        constexpr T&&
        release() noexcept
        {
            guarded_ = nullptr;
            return std::move(tmp_);
        }
    };

    template <class T, class U, class Vp>
    constexpr
    void
    ExpectedReinit(T* newval, U* oldval, Vp&& arg)
    noexcept(std::is_nothrow_constructible_v<T, Vp>)
    {
        if constexpr (std::is_nothrow_constructible_v<T, Vp>)
        {
            std::destroy_at(oldval);
            std::construct_at(newval, std::forward<Vp>(arg));
        }
        else if constexpr (std::is_nothrow_move_constructible_v<T>)
        {
            T tmp(std::forward<Vp>(arg)); // might throw
            std::destroy_at(oldval);
            std::construct_at(newval, std::move(tmp));
        }
        else
        {
            ExpectedGuard<U> guard(*oldval);
            std::construct_at(newval, std::forward<Vp>(arg)); // might throw
            guard.release();
        }
    }
}

} // namespace mrdocs::polyfill

#endif
