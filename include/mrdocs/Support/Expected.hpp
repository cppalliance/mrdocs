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

#ifndef MRDOCS_API_SUPPORT_EXPECTED_HPP
#define MRDOCS_API_SUPPORT_EXPECTED_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/source_location.hpp>
#include <mrdocs/Support/Error.hpp>
#include <fmt/format.h>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <functional>

namespace clang::mrdocs {

//------------------------------------------------
//
// Expected
//
//------------------------------------------------

template <class T, class E>
class Expected;

template <class E>
class Unexpected;

template <class E>
class BadExpectedAccess;

template <>
class BadExpectedAccess<void> : public std::exception
{
protected:
    BadExpectedAccess() noexcept = default;

    BadExpectedAccess(BadExpectedAccess const&) = default;

    BadExpectedAccess(BadExpectedAccess&&) = default;

    BadExpectedAccess&
    operator=(BadExpectedAccess const&) = default;

    BadExpectedAccess&
    operator=(BadExpectedAccess&&) = default;

    ~BadExpectedAccess() override = default;
public:
    [[nodiscard]]
    char const*
    what() const noexcept override
    {
        return "bad access to Expected without Expected value";
    }
};

template <class E>
class BadExpectedAccess : public BadExpectedAccess<void> {
    E unex_;
public:
    explicit
    BadExpectedAccess(E e)
        : unex_(std::move(e)) { }

    [[nodiscard]]
    E&
    error() & noexcept
    {
        return unex_;
    }

    [[nodiscard]]
    E const&
    error() const & noexcept
    {
        return unex_;
    }

    [[nodiscard]]
    E&&
    error() && noexcept
    {
        return std::move(unex_);
    }

    [[nodiscard]]
    E const&&
    error() const && noexcept
    {
        return std::move(unex_);
    }
};

struct unexpect_t
{
    explicit unexpect_t() = default;
};

inline constexpr unexpect_t unexpect{};

namespace detail
{
    template <class T>
    constexpr bool isExpected = false;
    template <class T, class E>
    constexpr bool isExpected<Expected<T, E>> = true;

    template <class T>
    constexpr bool isUnexpected = false;
    template <class T>
    constexpr bool isUnexpected<Unexpected<T>> = true;

    template <class Fn, class T>
    using then_result = std::remove_cvref_t<std::invoke_result_t<Fn&&, T&&>>;
    template <class Fn, class T>
    using result_transform = std::remove_cv_t<std::invoke_result_t<Fn&&, T&&>>;
    template <class Fn>
    using result0 = std::remove_cvref_t<std::invoke_result_t<Fn&&>>;
    template <class Fn>
    using result0_xform = std::remove_cv_t<std::invoke_result_t<Fn&&>>;

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
}

template <class E>
class Unexpected
{
    static_assert(detail::can_beUnexpected<E>);
    E unex_;

public:
    constexpr
    Unexpected(Unexpected const&) = default;

    constexpr
    Unexpected(Unexpected&&) = default;

    template <class Er = E>
    requires
      (!std::is_same_v<std::remove_cvref_t<Er>, Unexpected>) &&
      (!std::is_same_v<std::remove_cvref_t<Er>, std::in_place_t>) &&
        std::is_constructible_v<E, Er>
    constexpr explicit
    Unexpected(Er&& e) noexcept(std::is_nothrow_constructible_v<E, Er>)
        : unex_(std::forward<Er>(e))
    {}

    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr explicit
    Unexpected(std::in_place_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
    {}

    template <class U, class... Args>
    requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
    constexpr explicit
    Unexpected(
        std::in_place_t,
        std::initializer_list<U> il,
        Args&&... args)
    noexcept(std::is_nothrow_constructible_v<
        E, std::initializer_list<U>&, Args...>)
        : unex_(il, std::forward<Args>(args)...)
    {}

    constexpr Unexpected& operator=(Unexpected const&) = default;
    constexpr Unexpected& operator=(Unexpected&&) = default;

    [[nodiscard]]
    constexpr E const&
    error() const & noexcept
    {
        return unex_;
    }

    [[nodiscard]]
    constexpr E&
    error() & noexcept
    {
        return unex_;
    }

    [[nodiscard]]
    constexpr E const&&
    error() const && noexcept
    {
        return std::move(unex_);
    }

    [[nodiscard]]
    constexpr E&&
    error() && noexcept
    {
        return std::move(unex_);
    }

    constexpr
    void
    swap(Unexpected& other) noexcept(std::is_nothrow_swappable_v<E>)
    requires std::is_swappable_v<E>
    {
        using std::swap;
        swap(unex_, other.unex_);
    }

    template <class Er>
    [[nodiscard]]
    friend
    constexpr
    bool
    operator==(Unexpected const& x, const Unexpected<Er>& y)
    {
        return x.unex_ == y.error();
    }

    friend
    constexpr
    void
    swap(Unexpected& x, Unexpected& y)
    noexcept(noexcept(x.swap(y)))
    requires std::is_swappable_v<E>
    {
        x.swap(y);
    }
};

template <class E> Unexpected(E) -> Unexpected<E>;

namespace detail
{
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

/// Try to retrive expected-like type
#    define MRDOCS_TRY_VOID(expr)                          \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(detail::error(MRDOCS_UNIQUE_NAME)); \
        }                                                 \
        void(0)
#    define MRDOCS_TRY_VAR(var, expr)                      \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(detail::error(MRDOCS_UNIQUE_NAME)); \
        }                                                  \
        var = *std::move(MRDOCS_UNIQUE_NAME)
#    define MRDOCS_TRY_MSG(var, expr, msg)                 \
        auto MRDOCS_UNIQUE_NAME = expr;                    \
        if (detail::failed(MRDOCS_UNIQUE_NAME)) {          \
            return Unexpected(Error(msg));                 \
        }                                                  \
        var = *std::move(MRDOCS_UNIQUE_NAME)
#    define MRDOCS_TRY_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#    define MRDOCS_TRY(...) \
        MRDOCS_TRY_GET_MACRO(__VA_ARGS__, MRDOCS_TRY_MSG, MRDOCS_TRY_VAR, MRDOCS_TRY_VOID)(__VA_ARGS__)

/// Check existing expected-like type
#    define MRDOCS_CHECK_VOID(var)                         \
        if (detail::failed(var)) {                         \
            return Unexpected(detail::error(var));         \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_MSG(var, msg)                     \
        if (detail::failed(var)) {                         \
            return Unexpected(Error(msg));                 \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_GET_MACRO(_1, _2, NAME, ...) NAME
#    define MRDOCS_CHECK(...) \
        MRDOCS_CHECK_GET_MACRO(__VA_ARGS__, MRDOCS_CHECK_MSG, MRDOCS_CHECK_VOID)(__VA_ARGS__)

/// Check existing expected-like type and return custom value otherwise
#    define MRDOCS_CHECK_OR_VOID(var)                      \
        if (detail::failed(var)) {                         \
            return;                                        \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_OR_VALUE(var, value)              \
        if (detail::failed(var)) {                         \
            return value;                                  \
        }                                                  \
        void(0)
#    define MRDOCS_CHECK_GET_OR_MACRO(_1, _2, NAME, ...) NAME
#    define MRDOCS_CHECK_OR(...) \
        MRDOCS_CHECK_GET_OR_MACRO(__VA_ARGS__, MRDOCS_CHECK_OR_VALUE, MRDOCS_CHECK_OR_VOID)(__VA_ARGS__)

#    define MRDOCS_CHECK_OR_CONTINUE(var)                  \
        if (detail::failed(var)) {                         \
            continue;                                      \
        }                                                  \
        void(0)

#endif


/// @cond undocumented
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
/// @endcond

/** A container holding an error or a value.
 */
template <class T, class E = Error>
class Expected
{
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_function_v<T>);
    static_assert(!std::is_same_v<std::remove_cv_t<T>, std::in_place_t>);
    static_assert(!std::is_same_v<std::remove_cv_t<T>, unexpect_t>);
    static_assert(!detail::isUnexpected<std::remove_cv_t<T>>);
    static_assert(detail::can_beUnexpected<E>);

    template <class U, class Er, class Unex = Unexpected<E>>
    static constexpr bool constructible_from_expected =
           std::constructible_from<T, Expected<U, Er>&> ||
           std::constructible_from<T, Expected<U, Er>> ||
           std::constructible_from<T, const Expected<U, Er>&> ||
           std::constructible_from<T, const Expected<U, Er>> ||
           std::convertible_to<Expected<U, Er>&, T> ||
           std::convertible_to<Expected<U, Er>, T> ||
           std::convertible_to<const Expected<U, Er>&, T> ||
           std::convertible_to<const Expected<U, Er>, T> ||
           std::constructible_from<Unex, Expected<U, Er>&> ||
           std::constructible_from<Unex, Expected<U, Er>> ||
           std::constructible_from<Unex, const Expected<U, Er>&> ||
           std::constructible_from<Unex, const Expected<U, Er>>;

    template <class U, class Er>
    constexpr static bool explicit_conv
      = (!std::convertible_to<U, T>) ||
        (!std::convertible_to<Er, E>);

    template <class U>
    static constexpr bool same_val
      = std::is_same_v<class U::value_type, T>;

    template <class U>
    static constexpr bool same_err
      = std::is_same_v<class U::error_type, E>;

    template <class, class> friend class Expected;

    union {
        T val_;
        E unex_;
    };

    bool has_value_;

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = Unexpected<E>;

    template <class U>
    using rebind = Expected<U, error_type>;

    constexpr
    Expected()
    noexcept(std::is_nothrow_default_constructible_v<T>)
    requires std::is_default_constructible_v<T>
        : val_()
        , has_value_(true)
    {}

    Expected(Expected const&) = default;

    constexpr
    Expected(Expected const& x)
    noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
    requires
        std::is_copy_constructible_v<T> &&
        std::is_copy_constructible_v<E> &&
        (!std::is_trivially_copy_constructible_v<T> ||
         !std::is_trivially_copy_constructible_v<E>)
        : has_value_(x.has_value_)
    {
        if (has_value_)
        {
            std::construct_at(std::addressof(val_), x.val_);
        }
        else
        {
            std::construct_at(std::addressof(unex_), x.unex_);
        }
    }

    Expected(Expected&&) = default;

    constexpr
    Expected(Expected&& x)
    noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
    requires
        std::is_move_constructible_v<T> &&
        std::is_move_constructible_v<E> &&
        (!std::is_trivially_move_constructible_v<T> ||
         !std::is_trivially_move_constructible_v<E>)
        : has_value_(x.has_value_)
    {
        if (has_value_)
        {
            std::construct_at(std::addressof(val_), std::move(x).val_);
        }
        else
        {
            std::construct_at(std::addressof(unex_), std::move(x).unex_);
        }
    }

    template <class U, class G>
    requires
        std::is_constructible_v<T, U const&> &&
        std::is_constructible_v<E, G const&> &&
        (!constructible_from_expected<U, G>)
    constexpr
    explicit(explicit_conv<U const&, G const&>)
    Expected(const Expected<U, G>& x)
    noexcept(
        std::is_nothrow_constructible_v<T, U const&> &&
        std::is_nothrow_constructible_v<E, G const&>)
        : has_value_(x.has_value_)
    {
        if (has_value_)
        {
            std::construct_at(std::addressof(val_), x.val_);
        }
        else
        {
            std::construct_at(std::addressof(unex_), x.unex_);
        }
    }

    template <class U, class G>
    requires
        std::is_constructible_v<T, U> &&
        std::is_constructible_v<E, G> &&
        (!constructible_from_expected<U, G>)
    constexpr
    explicit(explicit_conv<U, G>)
    Expected(Expected<U, G>&& x)
    noexcept(
        std::is_nothrow_constructible_v<T, U> &&
        std::is_nothrow_constructible_v<E, G>)
        : has_value_(x.has_value_)
    {
        if (has_value_)
        {
            std::construct_at(std::addressof(val_), std::move(x).val_);
        }
        else
        {
            std::construct_at(std::addressof(unex_), std::move(x).unex_);
        }
    }

    template <class U = T>
    requires
        (!std::is_same_v<std::remove_cvref_t<U>, Expected>) &&
        (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) &&
        (!detail::isUnexpected<std::remove_cvref_t<U>>) &&
        std::is_constructible_v<T, U>
    constexpr
    explicit(!std::is_convertible_v<U, T>)
    Expected(U&& v)
    noexcept(std::is_nothrow_constructible_v<T, U>)
        : val_(std::forward<U>(v))
        , has_value_(true)
    { }

    template <class G = E>
    requires std::is_constructible_v<E, G const&>
    constexpr
    explicit(!std::is_convertible_v<G const&, E>)
    Expected(const Unexpected<G>& u)
    noexcept(std::is_nothrow_constructible_v<E, G const&>)
        : unex_(u.error())
        , has_value_(false)
    { }

    template <class G = E>
    requires std::is_constructible_v<E, G>
    constexpr
    explicit(!std::is_convertible_v<G, E>)
    Expected(Unexpected<G>&& u)
    noexcept(std::is_nothrow_constructible_v<E, G>)
        : unex_(std::move(u).error())
        , has_value_(false)
    { }

    template <class... Args>
    requires std::is_constructible_v<T, Args...>
    constexpr explicit
    Expected(std::in_place_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : val_(std::forward<Args>(args)...)
        , has_value_(true)
    { }

    template <class U, class... Args>
    requires
        std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    constexpr explicit
    Expected(
        std::in_place_t,
        std::initializer_list<U> il,
        Args&&... args)
    noexcept(
        std::is_nothrow_constructible_v<
            T, std::initializer_list<U>&, Args...>)
        : val_(il, std::forward<Args>(args)...)
        , has_value_(true)
    { }

    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr explicit
    Expected(unexpect_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
        , has_value_(false)
    { }

    template <class U, class... Args>
    requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
    constexpr explicit
    Expected(
        unexpect_t,
        std::initializer_list<U> il,
        Args&&... args)
    noexcept(
        std::is_nothrow_constructible_v<
            E, std::initializer_list<U>&, Args...>)
        : unex_(il, std::forward<Args>(args)...)
        , has_value_(false)
    { }

    constexpr ~Expected() = default;

    constexpr ~Expected()
    requires
        (!std::is_trivially_destructible_v<T>)
        || (!std::is_trivially_destructible_v<E>)
    {
        if (has_value_)
        {
            std::destroy_at(std::addressof(val_));
        }
        else
        {
            std::destroy_at(std::addressof(unex_));
        }
    }

    Expected&
    operator=(Expected const&) = delete;

    constexpr
    Expected&
    operator=(Expected const& x)
    noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E> &&
        std::is_nothrow_copy_assignable_v<T> &&
        std::is_nothrow_copy_assignable_v<E>)
    requires
        std::is_copy_assignable_v<T> &&
        std::is_copy_constructible_v<T> &&
        std::is_copy_assignable_v<E> &&
        std::is_copy_constructible_v<E> &&
        (std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>)
    {
        if (x.has_value_)
        {
            this->assign_val_impl(x.val_);
        }
        else
        {
            this->assign_unex_impl(x.unex_);
        }
        return *this;
    }

    constexpr
    Expected&
    operator=(Expected&& x)
    noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E> &&
        std::is_nothrow_move_assignable_v<T> &&
        std::is_nothrow_move_assignable_v<E>)
    requires
        std::is_move_assignable_v<T> &&
        std::is_move_constructible_v<T> &&
        std::is_move_assignable_v<E> &&
        std::is_move_constructible_v<E> &&
        (std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>)
    {
        if (x.has_value_)
        {
            assign_val_impl(std::move(x.val_));
        }
        else
        {
            assign_unex_impl(std::move(x.unex_));
        }
        return *this;
    }

    template <class U = T>
    requires
        (!std::is_same_v<Expected, std::remove_cvref_t<U>>) &&
        (!detail::isUnexpected<std::remove_cvref_t<U>>) &&
        std::is_constructible_v<T, U> &&
        std::is_assignable_v<T&, U> &&
        (std::is_nothrow_constructible_v<T, U> ||
         std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>)
    constexpr
    Expected&
    operator=(U&& v)
    {
        assign_val_impl(std::forward<U>(v));
        return *this;
    }

    template <class G>
    requires
        std::is_constructible_v<E, G const&> &&
        std::is_assignable_v<E&, G const&> &&
        (std::is_nothrow_constructible_v<E, G const&> ||
         std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>)
    constexpr
    Expected&
    operator=(const Unexpected<G>& e)
    {
        assign_unex_impl(e.error());
        return *this;
    }

    template <class G>
    requires
        std::is_constructible_v<E, G> &&
        std::is_assignable_v<E&, G> &&
        (std::is_nothrow_constructible_v<E, G> ||
         std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>)
    constexpr
    Expected&
    operator=(Unexpected<G>&& e)
    {
        assign_unex_impl(std::move(e).error());
        return *this;
    }

    template <class... Args>
    requires std::is_nothrow_constructible_v<T, Args...>
    constexpr
    T&
    emplace(Args&&... args) noexcept
    {
        if (has_value_)
        {
            std::destroy_at(std::addressof(val_));
        }
        else
        {
            std::destroy_at(std::addressof(unex_));
            has_value_ = true;
        }
        std::construct_at(
            std::addressof(val_),
            std::forward<Args>(args)...);
        return val_;
    }

    template <class U, class... Args>
    requires
        std::is_nothrow_constructible_v<
            T, std::initializer_list<U>&, Args...>
    constexpr
    T&
    emplace(std::initializer_list<U> il, Args&&... args) noexcept
    {
        if (has_value_)
        {
            std::destroy_at(std::addressof(val_));
        }
        else
        {
            std::destroy_at(std::addressof(unex_));
            has_value_ = true;
        }
        std::construct_at(
            std::addressof(val_), il,
            std::forward<Args>(args)...);
        return val_;
    }

    constexpr
    void
    swap(Expected& x)
    noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E> &&
        std::is_nothrow_swappable_v<T&> &&
        std::is_nothrow_swappable_v<E&>)
    requires
        std::is_swappable_v<T> &&
        std::is_swappable_v<E> &&
        std::is_move_constructible_v<T> &&
        std::is_move_constructible_v<E> &&
        (std::is_nothrow_move_constructible_v<T> ||
         std::is_nothrow_move_constructible_v<E>)
    {
        if (has_value_)
        {
            if (x.has_value_)
            {
                using std::swap;
                swap(val_, x.val_);
            }
            else
            {
                this->swap_val_unex_impl(x);
            }
        }
        else
        {
            if (x.has_value_)
            {
                x.swap_val_unex_impl(*this);
            }
            else
            {
                using std::swap;
                swap(unex_, x.unex_);
            }
        }
    }

    // observers

    [[nodiscard]]
    constexpr
    T const*
    operator->() const noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::addressof(val_);
    }

    [[nodiscard]]
    constexpr
    T*
    operator->() noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::addressof(val_);
    }

    [[nodiscard]]
    constexpr
    T const&
    operator*() const & noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return val_;
    }

    [[nodiscard]]
    constexpr
    T&
    operator*() & noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return val_;
    }

    [[nodiscard]]
    constexpr
    T const&&
    operator*() const && noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::move(val_);
    }

    [[nodiscard]]
    constexpr
    T&&
    operator*() && noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::move(val_);
    }

    [[nodiscard]]
    constexpr explicit
    operator bool() const noexcept
    {
        return has_value_;
    }

    [[nodiscard]]
    constexpr bool has_value() const noexcept
    {
        return has_value_;
    }

    constexpr T const&
    value() const &
    {
        if (has_value_) [[likely]]
        {
            return val_;
        }
        throw BadExpectedAccess<E>(unex_);
    }

    constexpr T&
    value() &
    {
        if (has_value_) [[likely]]
        {
            return val_;
        }
        auto const& unex = unex_;
        throw BadExpectedAccess<E>(unex);
    }

    constexpr T const&&
    value() const &&
    {
        if (has_value_) [[likely]]
        {
            return std::move(val_);
        }
        throw BadExpectedAccess<E>(std::move(unex_));
    }

    constexpr T&&
    value() &&
    {
        if (has_value_) [[likely]]
        {
            return std::move(val_);
        }
        throw BadExpectedAccess<E>(std::move(unex_));
    }

    constexpr E const&
    error() const & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    constexpr E&
    error() & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    constexpr E const&&
    error() const && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    constexpr E&&
    error() && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    template <class U>
    constexpr
    T
    value_or(U&& v) const &
    noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_convertible_v<U, T>)
    {
        static_assert( std::is_copy_constructible_v<T> );
        static_assert( std::is_convertible_v<U, T> );
        if (has_value_)
        {
            return val_;
        }
        return static_cast<T>(std::forward<U>(v));
    }

    template <class U>
    constexpr
    T
    value_or(U&& v) &&
    noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_convertible_v<U, T>)
    {
        static_assert( std::is_move_constructible_v<T> );
        static_assert( std::is_convertible_v<U, T> );
        if (has_value_)
        {
            return std::move(val_);
        }
        return static_cast<T>(std::forward<U>(v));
    }

    template <class G = E>
    constexpr
    E
    error_or(G&& e) const&
    {
        static_assert(std::is_copy_constructible_v<E>);
        static_assert(std::is_convertible_v<G, E>);
        if (has_value_)
        {
            return std::forward<G>(e);
        }
        return unex_;
    }

    template <class G = E>
    constexpr E
    error_or(G&& e) &&
    {
        static_assert(std::is_move_constructible_v<E>);
        static_assert(std::is_convertible_v<G, E>);
        if (has_value_)
        {
            return std::forward<G>(e);
        }
        return std::move(unex_);
    }

    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    and_then(Fn&& f) &
    {
        using U = detail::then_result<Fn, T&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), val_);
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    and_then(Fn&& f) const &
    {
        using U = detail::then_result<Fn, T const&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), val_);
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    and_then(Fn&& f) &&
    {
        using U = detail::then_result<Fn, T&&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), std::move(val_));
        }
        else
        {
            return U(unexpect, std::move(unex_));
        }
    }


    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    and_then(Fn&& f) const &&
    {
        using U = detail::then_result<Fn, T const&&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), std::move(val_));
        }
        else
        {
            return U(unexpect, std::move(unex_));
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, T&>
    constexpr
    auto
    or_else(Fn&& f) &
    {
        using G = detail::then_result<Fn, E&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G(std::in_place, val_);
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, T const&>
    constexpr
    auto
    or_else(Fn&& f) const &
    {
        using G = detail::then_result<Fn, E const&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G(std::in_place, val_);
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, T>
    constexpr
    auto
    or_else(Fn&& f) &&
    {
      using G = detail::then_result<Fn, E&&>;
      static_assert(detail::isExpected<G>);
      static_assert(std::is_same_v<class G::value_type, T>);

      if (has_value())
      {
          return G(std::in_place, std::move(val_));
      }
      else
      {
          return std::invoke(std::forward<Fn>(f), std::move(unex_));
      }
    }

    template <class Fn>
    requires std::is_constructible_v<T, const T>
    constexpr
    auto
    or_else(Fn&& f) const &&
    {
        using G = detail::then_result<Fn, E const&&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G(std::in_place, std::move(val_));
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    transform(Fn&& f) &
    {
        using U = detail::result_transform<Fn, T&>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(
                in_place_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), val_);
                });
        }
        else
        {
            return Res(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    transform(Fn&& f) const &
    {
        using U = detail::result_transform<Fn, T const&>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(
                in_place_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), val_);
                });
        }
        else
        {
            return Res(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    transform(Fn&& f) &&
    {
        using U = detail::result_transform<Fn, T>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(
                in_place_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), std::move(val_));
                });
        }
        else
        {
            return Res(unexpect, std::move(unex_));
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    transform(Fn&& f) const &&
    {
        using U = detail::result_transform<Fn, const T>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(
                in_place_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), std::move(val_));
                });
        }
        else
        {
            return Res(unexpect, std::move(unex_));
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, T&>
    constexpr
    auto
    transform_error(Fn&& f) &
    {
        using G = detail::result_transform<Fn, E&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res(std::in_place, val_);
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), unex_);
                });
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, T const&>
    constexpr
    auto
    transform_error(Fn&& f) const &
    {
        using G = detail::result_transform<Fn, E const&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res(std::in_place, val_);
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), unex_);
                });
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, T>
    constexpr
    auto
    transform_error(Fn&& f) &&
    {
        using G = detail::result_transform<Fn, E&&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res(std::in_place, std::move(val_));
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), std::move(unex_));
                });
        }
    }

    template <class Fn>
    requires std::is_constructible_v<T, const T>
    constexpr
    auto
    transform_error(Fn&& f) const &&
    {
        using G = detail::result_transform<Fn, E const&&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res(std::in_place, std::move(val_));
        }
        else
        {
            return Res(unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), std::move(unex_));
                });
        }
    }

    template <class U, class E2>
    requires (!std::is_void_v<U>)
    friend
    constexpr
    bool
    operator==(Expected const& x, const Expected<U, E2>& y)
    noexcept(
        noexcept(bool(*x == *y)) &&
        noexcept(bool(x.error() == y.error())))
    {
        if (x.has_value())
        {
            return y.has_value() && bool(*x == *y);
        }
        else
        {
            return !y.has_value() && bool(x.error() == y.error());
        }
    }

    template <class U>
    friend
    constexpr
    bool
    operator==(Expected const& x, U const& v)
    noexcept(noexcept(bool(*x == v)))
    {
        return x.has_value() && bool(*x == v);
    }

    template <class E2>
    friend
    constexpr
    bool
    operator==(Expected const& x, const Unexpected<E2>& e)
    noexcept(noexcept(bool(x.error() == e.error())))
    {
        return !x.has_value() && bool(x.error() == e.error());
    }

    friend
    constexpr
    void
    swap(Expected& x, Expected& y)
    noexcept(noexcept(x.swap(y)))
    requires requires {x.swap(y);}
    {
        x.swap(y);
    }

private:
    template <class Vp>
    constexpr
    void
    assign_val_impl(Vp&& v)
    {
        if (has_value_)
        {
            val_ = std::forward<Vp>(v);
        }
        else
        {
            detail::ExpectedReinit(
                std::addressof(val_),
                std::addressof(unex_),
                std::forward<Vp>(v));
            has_value_ = true;
        }
    }

    template <class Vp>
    constexpr
    void
    assign_unex_impl(Vp&& v)
    {
        if (has_value_)
        {
            detail::ExpectedReinit(
                std::addressof(unex_),
                std::addressof(val_),
                std::forward<Vp>(v));
            has_value_ = false;
        }
        else
        {
            unex_ = std::forward<Vp>(v);
        }
    }

    constexpr
    void
    swap_val_unex_impl(Expected& rhs)
    noexcept(
        std::is_nothrow_move_constructible_v<E> &&
        std::is_nothrow_move_constructible_v<T>)
    {
        if constexpr (std::is_nothrow_move_constructible_v<E>)
        {
            detail::ExpectedGuard<E> guard(rhs.unex_);
            std::construct_at(
                std::addressof(rhs.val_),
                std::move(val_));
            rhs.has_value_ = true;
            std::destroy_at(std::addressof(val_));
            std::construct_at(std::addressof(unex_), guard.release());
            has_value_ = false;
        }
        else
        {
            detail::ExpectedGuard<T> guard(val_);
            std::construct_at(
                std::addressof(unex_),
                std::move(rhs.unex_));
            has_value_ = false;
            std::destroy_at(std::addressof(rhs.unex_));
            std::construct_at(std::addressof(rhs.val_), guard.release());
            rhs.has_value_ = true;
        }
    }

    using in_place_inv = detail::in_place_inv;
    using unexpect_inv = detail::unexpect_inv;

    template <class Fn>
    explicit constexpr
    Expected(in_place_inv, Fn&& fn)
        : val_(std::forward<Fn>(fn)()), has_value_(true)
    { }

    template <class Fn>
    explicit constexpr
    Expected(unexpect_inv, Fn&& fn)
        : unex_(std::forward<Fn>(fn)()), has_value_(false)
    { }
};

template <class T, class E>
requires std::is_void_v<T>
class Expected<T, E>
{
    static_assert( detail::can_beUnexpected<E> );

    template <class U, class Er, class Unex = Unexpected<E>>
    static constexpr bool constructible_from_expected =
        std::is_constructible_v<Unex, Expected<U, Er>&> ||
        std::is_constructible_v<Unex, Expected<U, Er>> ||
        std::is_constructible_v<Unex, const Expected<U, Er>&> ||
        std::is_constructible_v<Unex, const Expected<U, Er>>;

    template <class U>
    static constexpr bool same_val
      = std::is_same_v<class U::value_type, T>;

    template <class U>
    static constexpr bool same_err
      = std::is_same_v<class U::error_type, E>;

    template <class, class> friend class Expected;

    union {
        struct { } void_;
        E unex_;
    };

    bool has_value_;

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = Unexpected<E>;

    template <class U>
    using rebind = Expected<U, error_type>;

    constexpr
    Expected() noexcept
        : void_()
        , has_value_(true)
    { }

    Expected(Expected const&) = default;

    constexpr
    Expected(Expected const& x)
    noexcept(std::is_nothrow_copy_constructible_v<E>)
    requires
        std::is_copy_constructible_v<E> &&
        (!std::is_trivially_copy_constructible_v<E>)
        : void_()
        , has_value_(x.has_value_)
    {
        if (!has_value_)
        {
            std::construct_at(std::addressof(unex_), x.unex_);
        }
    }

    Expected(Expected&&) = default;

    constexpr
    Expected(Expected&& x)
    noexcept(std::is_nothrow_move_constructible_v<E>)
    requires
        std::is_move_constructible_v<E> &&
        (!std::is_trivially_move_constructible_v<E>)
        : void_(), has_value_(x.has_value_)
    {
        if (!has_value_)
        {
            std::construct_at(std::addressof(unex_), std::move(x).unex_);
        }
    }

    template <class U, class G>
    requires
        std::is_void_v<U> &&
        std::is_constructible_v<E, G const&> &&
        (!constructible_from_expected<U, G>)
    constexpr
    explicit(!std::is_convertible_v<G const&, E>)
    Expected(const Expected<U, G>& x)
    noexcept(std::is_nothrow_constructible_v<E, G const&>)
        : void_()
        , has_value_(x.has_value_)
    {
        if (!has_value_)
        {
            std::construct_at(std::addressof(unex_), x.unex_);
        }
    }

    template <class U, class G>
    requires
        std::is_void_v<U> &&
        std::is_constructible_v<E, G> &&
        (!constructible_from_expected<U, G>)
    constexpr
    explicit(!std::is_convertible_v<G, E>)
    Expected(Expected<U, G>&& x)
    noexcept(std::is_nothrow_constructible_v<E, G>)
        : void_()
        , has_value_(x.has_value_)
    {
        if (!has_value_)
        {
            std::construct_at(std::addressof(unex_), std::move(x).unex_);
        }
    }

    template <class G = E>
    requires std::is_constructible_v<E, G const&>
    constexpr
    explicit(!std::is_convertible_v<G const&, E>)
    Expected(const Unexpected<G>& u)
    noexcept(std::is_nothrow_constructible_v<E, G const&>)
        : unex_(u.error())
        , has_value_(false)
    { }

    template <class G = E>
    requires std::is_constructible_v<E, G>
    constexpr
    explicit(!std::is_convertible_v<G, E>)
    Expected(Unexpected<G>&& u)
    noexcept(std::is_nothrow_constructible_v<E, G>)
        : unex_(std::move(u).error()), has_value_(false)
    { }

    constexpr explicit
    Expected(std::in_place_t) noexcept
        : Expected()
    { }

    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr explicit
    Expected(unexpect_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
        , has_value_(false)
    { }

    template <class U, class... Args>
    requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
    constexpr explicit
    Expected(unexpect_t, std::initializer_list<U> il, Args&&... args)
    noexcept(
        std::is_nothrow_constructible_v<
            E, std::initializer_list<U>&, Args...>)
        : unex_(il, std::forward<Args>(args)...), has_value_(false)
    { }

    constexpr ~Expected() = default;

    constexpr ~Expected()
    requires (!std::is_trivially_destructible_v<E>)
    {
        if (!has_value_)
        {
            std::destroy_at(std::addressof(unex_));
        }
    }

    Expected& operator=(Expected const&) = delete;

    constexpr
    Expected&
    operator=(Expected const& x)
    noexcept(
        std::is_nothrow_copy_constructible_v<E> &&
        std::is_nothrow_copy_assignable_v<E>)
    requires
        std::is_copy_constructible_v<E> &&
        std::is_copy_assignable_v<E>
    {
        if (x.has_value_)
        {
            emplace();
        }
        else
        {
            assign_unex_impl(x.unex_);
        }
        return *this;
    }

    constexpr
    Expected&
    operator=(Expected&& x)
    noexcept(
        std::is_nothrow_move_constructible_v<E> &&
        std::is_nothrow_move_assignable_v<E>)
    requires
        std::is_move_constructible_v<E> &&
        std::is_move_assignable_v<E>
    {
        if (x.has_value_)
        {
            emplace();
        }
        else
        {
            assign_unex_impl(std::move(x.unex_));
        }
        return *this;
    }

    template <class G>
    requires
        std::is_constructible_v<E, G const&> &&
        std::is_assignable_v<E&, G const&>
    constexpr
    Expected&
    operator=(const Unexpected<G>& e)
    {
        assign_unex_impl(e.error());
        return *this;
    }

    template <class G>
    requires
        std::is_constructible_v<E, G> &&
        std::is_assignable_v<E&, G>
    constexpr
    Expected&
    operator=(Unexpected<G>&& e)
    {
        assign_unex_impl(std::move(e.error()));
        return *this;
    }

    constexpr
    void
    emplace() noexcept
    {
        if (!has_value_)
        {
            std::destroy_at(std::addressof(unex_));
            has_value_ = true;
        }
    }

    constexpr
    void
    swap(Expected& x)
    noexcept(
        std::is_nothrow_swappable_v<E&> &&
        std::is_nothrow_move_constructible_v<E>)
    requires
        std::is_swappable_v<E> &&
        std::is_move_constructible_v<E>
    {
        if (has_value_)
        {
            if (!x.has_value_)
            {
                std::construct_at(
                    std::addressof(unex_),
                    std::move(x.unex_));
                std::destroy_at(std::addressof(x.unex_));
                has_value_ = false;
                x.has_value_ = true;
            }
        }
        else
        {
            if (x.has_value_)
            {
                std::construct_at(
                    std::addressof(x.unex_),
                    std::move(unex_));
                std::destroy_at(std::addressof(unex_));
                has_value_ = true;
                x.has_value_ = false;
            }
            else
            {
                using std::swap;
                swap(unex_, x.unex_);
            }
        }
    }

    [[nodiscard]]
    constexpr
    explicit
    operator bool() const noexcept
    {
        return has_value_;
    }

    [[nodiscard]]
    constexpr
    bool has_value() const noexcept
    {
        return has_value_;
    }

    constexpr
    void
    operator*() const noexcept {
        MRDOCS_ASSERT(has_value_);
    }

    constexpr
    void
    value() const&
    {
        if (has_value_) [[likely]]
        {
            return;
        }
        throw BadExpectedAccess<E>(unex_);
    }

    constexpr
    void
    value() &&
    {
        if (has_value_) [[likely]]
        {
            return;
        }
        throw BadExpectedAccess<E>(std::move(unex_));
    }

    constexpr E const&
    error() const & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    constexpr E&
    error() & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    constexpr E const&&
    error() const && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    constexpr E&&
    error() && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    template <class G = E>
    constexpr E
    error_or(G&& e) const&
    {
      static_assert( std::is_copy_constructible_v<E> );
      static_assert( std::is_convertible_v<G, E> );

      if (has_value_)
      {
          return std::forward<G>(e);
      }
      return unex_;
    }

    template <class G = E>
    constexpr E
    error_or(G&& e) &&
    {
        static_assert( std::is_move_constructible_v<E> );
        static_assert( std::is_convertible_v<G, E> );

        if (has_value_)
        {
            return std::forward<G>(e);
        }
        return std::move(unex_);
    }

    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    and_then(Fn&& f) &
    {
        using U = detail::result0<Fn>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f));
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    and_then(Fn&& f) const &
    {
        using U = detail::result0<Fn>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f));
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    and_then(Fn&& f) &&
    {
      using U = detail::result0<Fn>;
      static_assert(detail::isExpected<U>);
      static_assert(std::is_same_v<class U::error_type, E>);

      if (has_value())
      {
          return std::invoke(std::forward<Fn>(f));
      }
      else
      {
          return U(unexpect, std::move(unex_));
      }
    }

    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    and_then(Fn&& f) const &&
    {
        using U = detail::result0<Fn>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<class U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f));
        }
        else
        {
            return U(unexpect, std::move(unex_));
        }
    }

    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) &
    {
        using G = detail::then_result<Fn, E&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) const &
    {
        using G = detail::then_result<Fn, E const&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) &&
    {
        using G = detail::then_result<Fn, E&&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        }
    }

    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) const&&
    {
        using G = detail::then_result<Fn, E const&&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<class G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    transform(Fn&& f) &
    {
        using U = detail::result0_xform<Fn>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(in_place_inv{}, std::forward<Fn>(f));
        }
        else
        {
            return Res(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    transform(Fn&& f) const &
    {
        using U = detail::result0_xform<Fn>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(in_place_inv{}, std::forward<Fn>(f));
        }
        else
        {
            return Res(unexpect, unex_);
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    transform(Fn&& f) &&
    {
        using U = detail::result0_xform<Fn>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(in_place_inv{}, std::forward<Fn>(f));
        }
        else
        {
            return Res(unexpect, std::move(unex_));
        }
    }

    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    transform(Fn&& f) const &&
    {
        using U = detail::result0_xform<Fn>;
        using Res = Expected<U, E>;

        if (has_value())
        {
            return Res(in_place_inv{}, std::forward<Fn>(f));
        }
        else
        {
            return Res(unexpect, std::move(unex_));
        }
    }

    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) &
    {
        using G = detail::result_transform<Fn, E&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res();
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), unex_);
                });
        }
    }

    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) const &
    {
        using G = detail::result_transform<Fn, E const&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res();
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), unex_);
                });
        }
    }

    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) &&
    {
        using G = detail::result_transform<Fn, E&&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res();
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), std::move(unex_));
                });
        }
    }

    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) const &&
    {
        using G = detail::result_transform<Fn, E const&&>;
        using Res = Expected<T, G>;

        if (has_value())
        {
            return Res();
        }
        else
        {
            return Res(
                unexpect_inv{}, [&]() {
                    return std::invoke(std::forward<Fn>(f), std::move(unex_));
                });
        }
    }

    template <class U, class E2>
    requires std::is_void_v<U>
    friend
    constexpr
    bool
    operator==(Expected const& x, const Expected<U, E2>& y)
    noexcept(noexcept(bool(x.error() == y.error())))
    {
        if (x.has_value())
        {
            return y.has_value();
        }
        else
        {
            return !y.has_value() && bool(x.error() == y.error());
        }
    }

    template <class E2>
    friend
    constexpr
    bool
    operator==(Expected const& x, const Unexpected<E2>& e)
    noexcept(noexcept(bool(x.error() == e.error())))
    {
        return !x.has_value() && bool(x.error() == e.error());
    }

    friend
    constexpr
    void
    swap(Expected& x, Expected& y)
    noexcept(noexcept(x.swap(y)))
    requires requires { x.swap(y); }
    {
        x.swap(y);
    }

private:
    template <class Vp>
    constexpr
    void
    assign_unex_impl(Vp&& v)
    {
        if (has_value_)
        {
            std::construct_at(
                std::addressof(unex_),
                std::forward<Vp>(v));
            has_value_ = false;
        }
        else
        {
            unex_ = std::forward<Vp>(v);
        }
    }

    using in_place_inv = detail::in_place_inv;
    using unexpect_inv = detail::unexpect_inv;

    template <class Fn>
    explicit constexpr
    Expected(in_place_inv, Fn&& fn)
        : void_()
        , has_value_(true)
    {
        std::forward<Fn>(fn)();
    }

    template <class Fn>
    explicit constexpr
    Expected(unexpect_inv, Fn&& fn)
        : unex_(std::forward<Fn>(fn)())
        , has_value_(false)
    { }
};

} // clang::mrdocs

#endif
