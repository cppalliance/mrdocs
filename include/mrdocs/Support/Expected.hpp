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
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/source_location.hpp>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mrdocs {

//------------------------------------------------
//
// Expected
//
//------------------------------------------------

template <class T, class E>
class Expected;

template <class E>
class Unexpected;

/** Base class for accessing an empty Expected.
*/
template <class E>
class BadExpectedAccess;

/** Exception thrown when reading the value of an empty Expected.
*/
template <>
class BadExpectedAccess<void> : public std::exception
{
protected:
    /// Default constructor.
    BadExpectedAccess() noexcept = default;

    /// Copy constructor.
    BadExpectedAccess(BadExpectedAccess const&) = default;

    /// Move constructor.
    BadExpectedAccess(BadExpectedAccess&&) = default;

    /// Copy assignment.
    BadExpectedAccess&
    operator=(BadExpectedAccess const&) = default;

    /// Move assignment.
    BadExpectedAccess&
    operator=(BadExpectedAccess&&) = default;

    /// Virtual destructor.
    ~BadExpectedAccess() override = default;
public:
    /** Return a diagnostic string.
    */
    [[nodiscard]]
    char const*
    what() const noexcept override
    {
        return "bad access to Expected without Expected value";
    }
};

/** Exception thrown when reading the error of an Expected with a value.
*/
template <class E>
class BadExpectedAccess : public BadExpectedAccess<void> {
    E unex_;
public:
    /** Construct with the unexpected error value.
     */
    explicit
    BadExpectedAccess(E e)
        : unex_(std::move(e)) { }

    /** Access the contained error by lvalue reference.
        @return Reference to the stored unexpected value.
    */
    [[nodiscard]]
    E&
    error() & noexcept
    {
        return unex_;
    }

    /** Access the contained error by const lvalue reference.
        @return Const reference to the stored unexpected value.
    */
    [[nodiscard]]
    E const&
    error() const & noexcept
    {
        return unex_;
    }

    /** Access the contained error by rvalue reference.
        @return Rvalue reference to the stored unexpected value.
    */
    [[nodiscard]]
    E&&
    error() && noexcept
    {
        return std::move(unex_);
    }

    /** Access the contained error by const rvalue reference.
        @return Const rvalue reference to the stored unexpected value.
    */
    [[nodiscard]]
    E const&&
    error() const && noexcept
    {
        return std::move(unex_);
    }
};

/** Tag type used to select unexpected construction.
*/
struct unexpect_t
{
    /// Default constructor.
    explicit unexpect_t() = default;
};

/** Tag object to request unexpected construction.
*/
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

/** Holds an unexpected error value for Expected.
*/
template <class E>
class Unexpected
{
    static_assert(detail::can_beUnexpected<E>);
    E unex_;

public:
    /// Copy constructor.
    constexpr
    Unexpected(Unexpected const&) = default;

    /// Move constructor.
    constexpr
    Unexpected(Unexpected&&) = default;

    /** Construct from an error value convertible to `E`.
        @param e Error value to store.
    */
    template <class Er = E>
    requires
      (!std::is_same_v<std::remove_cvref_t<Er>, Unexpected>) &&
      (!std::is_same_v<std::remove_cvref_t<Er>, std::in_place_t>) &&
        std::is_constructible_v<E, Er>
    constexpr explicit
    Unexpected(Er&& e) noexcept(std::is_nothrow_constructible_v<E, Er>)
        : unex_(std::forward<Er>(e))
    {}

    /** In-place construct the error value with arguments.
        @param in_place Tag selecting in-place construction.
        @param args Arguments forwarded to `E`'s constructor.
    */
    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr explicit
    Unexpected(std::in_place_t in_place, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
    {
        (void)in_place;
    }

    /** In-place construct the error value from an initializer list.
        @param in_place Tag selecting in-place construction.
        @param il Initializer list for the error.
        @param args Additional constructor arguments.
    */
    template <class U, class... Args>
    requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
    constexpr explicit
    Unexpected(
        std::in_place_t in_place,
        std::initializer_list<U> il,
        Args&&... args)
    noexcept(std::is_nothrow_constructible_v<
        E, std::initializer_list<U>&, Args...>)
        : unex_(il, std::forward<Args>(args)...)
    {
        (void)in_place;
    }

    /// Copy assignment.
    constexpr Unexpected& operator=(Unexpected const&) = default;
    /// Move assignment.
    constexpr Unexpected& operator=(Unexpected&&) = default;

    /** Return a const reference to the stored error.
        @return Const reference to `E`.
    */
    [[nodiscard]]
    constexpr E const&
    error() const & noexcept
    {
        return unex_;
    }

    /** Return a reference to the stored error.
        @return Reference to `E`.
    */
    [[nodiscard]]
    constexpr E&
    error() & noexcept
    {
        return unex_;
    }

    /** Return a const rvalue reference to the stored error.
        @return Const rvalue reference to `E`.
    */
    [[nodiscard]]
    constexpr E const&&
    error() const && noexcept
    {
        return std::move(unex_);
    }

    /** Return a rvalue reference to the stored error.
        @return Rvalue reference to `E`.
    */
    [[nodiscard]]
    constexpr E&&
    error() && noexcept
    {
        return std::move(unex_);
    }

    /** Swap the contained error with another instance.
    */
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

/** Deduction guide for Unexpected, forwarding the error type.
*/
template <class E>
Unexpected(E) -> Unexpected<E>;

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

#endif


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

/** A container holding an error or a value.
*/
/** Monadic result type holding either a value `T` or an unexpected error `E`.
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
      = std::is_same_v<typename U::error_type, E>;

    template <class, class> friend class Expected;

    union {
        /// Storage for the engaged value.
        T val_;
        /// Storage for the unexpected error.
        E unex_;
    };

    /// True when the value alternative is active.
    bool has_value_;

public:
    /// Type produced on success.
    using value_type = T;
    /// Type produced on failure.
    using error_type = E;
    /// Convenience alias for an Unexpected containing the error type.
    using unexpected_type = Unexpected<E>;

    template <class U>
    /// Rebind to an `Expected` with a different value type and the same error type.
    using rebind = Expected<U, error_type>;

    /// Construct an engaged Expected with a default-initialized value.
    constexpr
    Expected()
    noexcept(std::is_nothrow_default_constructible_v<T>)
    requires std::is_default_constructible_v<T>
        : val_()
        , has_value_(true)
    {}

    /// Default copy constructor.
    Expected(Expected const&) = default;

    /** Copy-construct, handling non-trivial alternatives.
    */
    /** Copy-construct from another void Expected.
    */
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

    /// Default move constructor.
    /// Default move constructor.
    Expected(Expected&&) = default;

    /** Move-construct, handling non-trivial alternatives.
    */
    /** Move-construct from another void Expected.
    */
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

    /** Construct from another Expected with potentially different types.
    */
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

    /** Move-construct from another Expected with potentially different types.
    */
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

    /** Construct an engaged Expected from a convertible value.
    */
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

    /** Construct a disengaged Expected from an unexpected error (copy).
    */
    template <class G = E>
    requires std::is_constructible_v<E, G const&>
    constexpr
    explicit(!std::is_convertible_v<G const&, E>)
    Expected(const Unexpected<G>& u)
    noexcept(std::is_nothrow_constructible_v<E, G const&>)
        : unex_(u.error())
        , has_value_(false)
    { }

    /** Construct a disengaged Expected from an unexpected error (move).
    */
    template <class G = E>
    requires std::is_constructible_v<E, G>
    constexpr
    explicit(!std::is_convertible_v<G, E>)
    Expected(Unexpected<G>&& u)
    noexcept(std::is_nothrow_constructible_v<E, G>)
        : unex_(std::move(u).error())
        , has_value_(false)
    { }

    /** Construct an engaged Expected in-place.
        @param args Arguments forwarded to the value constructor.
    */
    template <class... Args>
    requires std::is_constructible_v<T, Args...>
    constexpr explicit
    Expected(std::in_place_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : val_(std::forward<Args>(args)...)
        , has_value_(true)
    {
    }

    /** Construct an engaged Expected from an initializer list.
        @param il Initializer list forwarded to the value constructor.
        @param args Additional arguments forwarded to the value constructor.
    */
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
    {
    }

    /** Construct a disengaged Expected holding an error.
        @param args Arguments forwarded to the error constructor.
    */
    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr explicit
    Expected(unexpect_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
        , has_value_(false)
    {
    }

    /** Construct a disengaged Expected from an initializer list.
        @param il Initializer list forwarded to the error constructor.
        @param args Additional arguments forwarded to the error constructor.
    */
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
    {
    }

    /// Defaulted trivial destructor when both alternatives are trivial.
    constexpr ~Expected() = default;

    /** Destroy the active alternative when a non-trivial destructor is required.
    */
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

    /** Copy-assign from another Expected.
    */
    Expected&
    operator=(Expected const&) = delete;

    /** Assign from another Expected with compatible value/error types.
    */
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

    /** Move-assign from another Expected with compatible value/error types.
    */
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

    /** Assign a new value, reconstructing if currently disengaged.
    */
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

    /** Assign a new unexpected error from lvalue.
    */
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

    /** Assign a new unexpected error from rvalue.
    */
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

    /** Reconstruct the value in-place, discarding any current state.
        @param args Arguments forwarded to the value constructor.
        @return Reference to the newly emplaced value.
    */
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

    /** Reconstruct the value from an initializer list.
        @param il Initializer list forwarded to the value constructor.
        @param args Additional constructor arguments.
        @return Reference to the newly emplaced value.
    */
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

    /** Swap the stored state with another Expected.
    */
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

    /** Access value members through pointer syntax.
        @return Pointer to the contained value.
    */
    [[nodiscard]]
    constexpr
    T const*
    operator->() const noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::addressof(val_);
    }

    /** Access value members through pointer syntax.
        @return Pointer to the contained value.
    */
    [[nodiscard]]
    constexpr
    T*
    operator->() noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::addressof(val_);
    }

    /** Dereference to a const lvalue value reference.
        @return Reference to the contained value.
    */
    [[nodiscard]]
    constexpr
    T const&
    operator*() const & noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return val_;
    }

    /** Dereference to an lvalue value reference.
        @return Reference to the contained value.
    */
    [[nodiscard]]
    constexpr
    T&
    operator*() & noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return val_;
    }

    /** Dereference to a const rvalue value reference.
        @return Reference to the contained value.
    */
    [[nodiscard]]
    constexpr
    T const&&
    operator*() const && noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::move(val_);
    }

    /** Dereference to an rvalue value reference.
        @return Reference to the contained value.
    */
    [[nodiscard]]
    constexpr
    T&&
    operator*() && noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::move(val_);
    }

    /** Return true when the Expected contains a value.
    */
    [[nodiscard]]
    constexpr explicit
    operator bool() const noexcept
    {
        return has_value_;
    }

    /** Return true when the Expected contains a value.
    */
    [[nodiscard]]
    constexpr bool has_value() const noexcept
    {
        return has_value_;
    }

    /** Access the stored value or throw BadExpectedAccess.
        @return Reference to the contained value.
    */
    constexpr T const&
    value() const &
    {
        if (has_value_) [[likely]]
        {
            return val_;
        }
        throw BadExpectedAccess<E>(unex_);
    }

    /** Access the stored value or throw BadExpectedAccess.
        @return Reference to the contained value.
    */
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

    /** Access the stored value or throw BadExpectedAccess (rvalue overload).
        @return Rvalue reference to the contained value.
    */
    constexpr T const&&
    value() const &&
    {
        if (has_value_) [[likely]]
        {
            return std::move(val_);
        }
        throw BadExpectedAccess<E>(std::move(unex_));
    }

    /** Access the stored value or throw BadExpectedAccess (rvalue overload).
        @return Rvalue reference to the contained value.
    */
    constexpr T&&
    value() &&
    {
        if (has_value_) [[likely]]
        {
            return std::move(val_);
        }
        throw BadExpectedAccess<E>(std::move(unex_));
    }

    /** Access the stored error; precondition: !has_value().
        @return Reference to the contained error.
    */
    constexpr E const&
    error() const & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    /** Access the stored error; precondition: !has_value().
        @return Reference to the contained error.
    */
    constexpr E&
    error() & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    /** Access the stored error; precondition: !has_value().
        @return Rvalue reference to the contained error.
    */
    constexpr E const&&
    error() const && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    /** Access the stored error; precondition: !has_value().
        @return Rvalue reference to the contained error.
    */
    constexpr E&&
    error() && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    /** Return the contained value or a fallback copy.
        @param v Fallback value to use when disengaged.
        @return Contained value or the fallback converted to `T`.
    */
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

    /** Return the contained value or a fallback move.
        @param v Fallback value to use when disengaged.
        @return Contained value or the fallback converted to `T`.
    */
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

    /** Return the contained error or a fallback copy.
        @param e Fallback error to use when engaged.
        @return Contained error or the fallback converted to `E`.
    */
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

    /** Return the contained error or a fallback move.
        @param e Fallback error to use when engaged.
        @return Contained error or the fallback converted to `E`.
    */
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

    /** Invoke `f` when engaged, propagate error otherwise.
        @param f Continuation that returns another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    and_then(Fn&& f) &
    {
        using U = detail::ThenResult<Fn, T&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), val_);
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    /** Invoke `f` when engaged (const lvalue), propagate error otherwise.
        @param f Continuation that returns another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    and_then(Fn&& f) const &
    {
        using U = detail::ThenResult<Fn, T const&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), val_);
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    /** Invoke `f` when engaged (rvalue), propagate error otherwise.
        @param f Continuation that returns another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    and_then(Fn&& f) &&
    {
        using U = detail::ThenResult<Fn, T&&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), std::move(val_));
        }
        else
        {
            return U(unexpect, std::move(unex_));
        }
    }


    /** Invoke `f` when engaged (const rvalue), propagate error otherwise.
        @param f Continuation that returns another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    and_then(Fn&& f) const &&
    {
        using U = detail::ThenResult<Fn, T const&&>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f), std::move(val_));
        }
        else
        {
            return U(unexpect, std::move(unex_));
        }
    }

    /** Invoke `f` when in error, otherwise return current value.
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    requires std::is_constructible_v<T, T&>
    constexpr
    auto
    or_else(Fn&& f) &
    {
        using G = detail::ThenResult<Fn, E&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G(std::in_place, val_);
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    /** Invoke `f` when in error (const lvalue).
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    requires std::is_constructible_v<T, T const&>
    constexpr
    auto
    or_else(Fn&& f) const &
    {
        using G = detail::ThenResult<Fn, E const&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G(std::in_place, val_);
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    /** Invoke `f` when in error (rvalue).
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    requires std::is_constructible_v<T, T>
    constexpr
    auto
    or_else(Fn&& f) &&
    {
      using G = detail::ThenResult<Fn, E&&>;
      static_assert(detail::isExpected<G>);
      static_assert(std::is_same_v<typename G::value_type, T>);

      if (has_value())
      {
          return G(std::in_place, std::move(val_));
      }
      else
      {
          return std::invoke(std::forward<Fn>(f), std::move(unex_));
      }
    }

    /** Invoke `f` when in error (const rvalue).
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    requires std::is_constructible_v<T, const T>
    constexpr
    auto
    or_else(Fn&& f) const &&
    {
        using G = detail::ThenResult<Fn, E const&&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G(std::in_place, std::move(val_));
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        }
    }

    /** Map the contained value, propagate error.
        @param f Mapping function applied to the value.
        @return Expected holding the mapped value or the original error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    transform(Fn&& f) &
    {
        using U = detail::ResultTransform<Fn, T&>;
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

    /** Map the contained value (const overload), propagate error.
        @param f Mapping function applied to the value.
        @return Expected holding the mapped value or the original error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    transform(Fn&& f) const &
    {
        using U = detail::ResultTransform<Fn, T const&>;
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

    /** Map the contained value (rvalue), propagate error.
        @param f Mapping function applied to the value.
        @return Expected holding the mapped value or the original error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    transform(Fn&& f) &&
    {
        using U = detail::ResultTransform<Fn, T>;
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

    /** Map the contained value (const rvalue), propagate error.
        @param f Mapping function applied to the value.
        @return Expected holding the mapped value or the original error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    transform(Fn&& f) const &&
    {
        using U = detail::ResultTransform<Fn, const T>;
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

    /** Map the contained error, preserve value.
        @param f Mapping function applied to the error.
        @return Expected holding the original value or mapped error.
    */
    template <class Fn>
    requires std::is_constructible_v<T, T&>
    constexpr
    auto
    transform_error(Fn&& f) &
    {
        using G = detail::ResultTransform<Fn, E&>;
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

    /** Map the contained error (const lvalue), preserve value.
        @param f Mapping function applied to the error.
        @return Expected holding the original value or mapped error.
    */
    template <class Fn>
    requires std::is_constructible_v<T, T const&>
    constexpr
    auto
    transform_error(Fn&& f) const &
    {
        using G = detail::ResultTransform<Fn, E const&>;
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

    /** Map the contained error (rvalue), preserve value.
        @param f Mapping function applied to the error.
        @return Expected holding the original value or mapped error.
    */
    template <class Fn>
    requires std::is_constructible_v<T, T>
    constexpr
    auto
    transform_error(Fn&& f) &&
    {
        using G = detail::ResultTransform<Fn, E&&>;
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

    /** Map the contained error (const rvalue), preserve value.
        @param f Mapping function applied to the error.
        @return Expected holding the original value or mapped error.
    */
    template <class Fn>
    requires std::is_constructible_v<T, const T>
    constexpr
    auto
    transform_error(Fn&& f) const &&
    {
        using G = detail::ResultTransform<Fn, E const&&>;
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

    /** Swap contents with another Expected.
    */
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

/** Expected specialization for `void` values.
    Holds either success (no payload) or an unexpected error `E`.
*/
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
      = std::is_same_v<typename U::error_type, E>;

    template <class, class> friend class Expected;

    struct engaged_state { };

    union {
        /** Placeholder for the engaged state.
        */
        engaged_state void_;
        /** Stored unexpected error.
        */
        E unex_;
    };

    /// True when the expected is engaged.
    bool has_value_;

public:
    /// Value type for this specialization (always void).
    using value_type = T;
    /// Error type stored when disengaged.
    using error_type = E;
    /// Alias for the unexpected wrapper.
    using unexpected_type = Unexpected<E>;

    /// Rebind to another value type with the same error type.
    template <class U>
    using rebind = Expected<U, error_type>;

    /// Construct an engaged Expected<void>.
    constexpr
    Expected() noexcept
        : void_()
        , has_value_(true)
    { }

    /// Default copy constructor.
    Expected(Expected const&) = default;

    /** Copy-construct with explicit error handling for non-trivial `E`.
    */
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

    /// Default move constructor.
    Expected(Expected&&) = default;

    /** Move-construct with explicit error handling for non-trivial `E`.
    */
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

    /** Copy-construct from a compatible Expected carrying void.
    */
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

    /** Move-construct from a compatible Expected carrying void.
    */
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

    /** Construct a disengaged Expected from an unexpected error (copy).
    */
    template <class G = E>
    requires std::is_constructible_v<E, G const&>
    constexpr
    explicit(!std::is_convertible_v<G const&, E>)
    Expected(const Unexpected<G>& u)
    noexcept(std::is_nothrow_constructible_v<E, G const&>)
        : unex_(u.error())
        , has_value_(false)
    { }

    /** Construct a disengaged Expected from an unexpected error (move).
    */
    template <class G = E>
    requires std::is_constructible_v<E, G>
    constexpr
    explicit(!std::is_convertible_v<G, E>)
    Expected(Unexpected<G>&& u)
    noexcept(std::is_nothrow_constructible_v<E, G>)
        : unex_(std::move(u).error()), has_value_(false)
    { }

    /** Construct an engaged Expected with in-place tag.
    */
    constexpr explicit
    Expected(std::in_place_t) noexcept
        : Expected()
    {
    }

    /** Construct a disengaged Expected from error arguments.
        @param args Arguments forwarded to the error constructor.
    */
    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr explicit
    Expected(unexpect_t, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
        , has_value_(false)
    {
    }

    /** Construct a disengaged Expected from an initializer list of errors.
        @param il Initializer list forwarded to the error constructor.
        @param args Additional arguments forwarded to the error constructor.
    */
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
        : unex_(il, std::forward<Args>(args)...), has_value_(false)
    {
    }

    /// Defaulted trivial destructor.
    constexpr ~Expected() = default;

    /// Destroy the stored error when non-trivial.
    constexpr ~Expected()
    requires (!std::is_trivially_destructible_v<E>)
    {
        if (!has_value_)
        {
            std::destroy_at(std::addressof(unex_));
        }
    }

    /// Copy assignment disabled to keep semantics explicit.
    Expected& operator=(Expected const&) = delete;

    /** Copy-assign from another void Expected.
    */
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

    /** Move-assign from another void Expected.
    */
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

    /** Assign a new unexpected error (lvalue).
    */
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

    /** Assign a new unexpected error (rvalue).
    */
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

    /** Reset to engaged state (no error).
    */
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

    /** Swap states with another void Expected.
        @param x Other instance to exchange with.
    */
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

    /** Return true when the Expected holds a value.
    */
    [[nodiscard]]
    constexpr
    explicit
    operator bool() const noexcept
    {
        return has_value_;
    }

    /** Return true when the Expected holds a value.
    */
    [[nodiscard]]
    constexpr
    bool has_value() const noexcept
    {
        return has_value_;
    }

    /** Ensure the Expected is engaged; throws if it holds an error.
    */
    constexpr
    void
    operator*() const noexcept {
        MRDOCS_ASSERT(has_value_);
    }

    /** Ensure the Expected is engaged; throws BadExpectedAccess if not.
    */
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

    /** Ensure the Expected is engaged; throws BadExpectedAccess if not (rvalue overload).
    */
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

    /** Return a const reference to the contained error; precondition: disengaged.
    */
    constexpr E const&
    error() const & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    /** Return a reference to the contained error; precondition: disengaged.
    */
    constexpr E&
    error() & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    /** Return an rvalue reference to the contained error; precondition: disengaged.
    */
    constexpr E const&&
    error() const && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    /** Return an rvalue reference to the contained error; precondition: disengaged.
    */
    constexpr E&&
    error() && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    /** Return the error or a fallback copy.
        @param e Fallback to return if engaged.
        @return Contained error or the fallback converted to `E`.
    */
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

    /** Return the error or a fallback move.
        @param e Fallback to return if engaged.
        @return Contained error or the fallback converted to `E`.
    */
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

    /** Invoke `f` when engaged, propagate error otherwise.
        @param f Continuation returning another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    and_then(Fn&& f) &
    {
        using U = detail::result0<Fn>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f));
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    /** Invoke `f` when engaged (const lvalue), propagate error otherwise.
        @param f Continuation returning another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    and_then(Fn&& f) const &
    {
        using U = detail::result0<Fn>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f));
        }
        else
        {
            return U(unexpect, unex_);
        }
    }

    /** Invoke `f` when engaged (rvalue), propagate error otherwise.
        @param f Continuation returning another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    and_then(Fn&& f) &&
    {
      using U = detail::result0<Fn>;
      static_assert(detail::isExpected<U>);
      static_assert(std::is_same_v<typename U::error_type, E>);

      if (has_value())
      {
          return std::invoke(std::forward<Fn>(f));
      }
      else
      {
          return U(unexpect, std::move(unex_));
      }
    }

    /** Invoke `f` when engaged (const rvalue), propagate error otherwise.
        @param f Continuation returning another Expected.
        @return Result of `f` or this error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    and_then(Fn&& f) const &&
    {
        using U = detail::result0<Fn>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);

        if (has_value())
        {
            return std::invoke(std::forward<Fn>(f));
        }
        else
        {
            return U(unexpect, std::move(unex_));
        }
    }

    /** Invoke `f` when in error, otherwise return current value.
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) &
    {
        using G = detail::ThenResult<Fn, E&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    /** Invoke `f` when in error (const lvalue), otherwise return current value.
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) const &
    {
        using G = detail::ThenResult<Fn, E const&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), unex_);
        }
    }

    /** Invoke `f` when in error (rvalue), otherwise return current value.
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) &&
    {
        using G = detail::ThenResult<Fn, E&&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        }
    }

    /** Invoke `f` when in error (const rvalue), otherwise return current value.
        @param f Recovery function returning an Expected.
        @return Current value or result of `f`.
    */
    template <class Fn>
    constexpr
    auto
    or_else(Fn&& f) const&&
    {
        using G = detail::ThenResult<Fn, E const&&>;
        static_assert(detail::isExpected<G>);
        static_assert(std::is_same_v<typename G::value_type, T>);

        if (has_value())
        {
            return G();
        }
        else
        {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        }
    }

    /** Transform the contained error type when engaged (lvalue).
        @param f Transformation to apply.
        @return New Expected produced from `f` or the existing error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E&>
    constexpr
    auto
    transform(Fn&& f) &
    {
        using U = detail::result0Transform<Fn>;
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

    /** Transform the contained error type when engaged (const lvalue).
        @param f Transformation to apply.
        @return New Expected produced from `f` or the existing error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E const&>
    constexpr
    auto
    transform(Fn&& f) const &
    {
        using U = detail::result0Transform<Fn>;
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

    /** Transform the contained error type when engaged (rvalue).
        @param f Transformation to apply.
        @return New Expected produced from `f` or the existing error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, E>
    constexpr
    auto
    transform(Fn&& f) &&
    {
        using U = detail::result0Transform<Fn>;
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

    /** Transform the contained error type when engaged (const rvalue).
        @param f Transformation to apply.
        @return New Expected produced from `f` or the existing error.
    */
    template <class Fn>
    requires std::is_constructible_v<E, const E>
    constexpr
    auto
    transform(Fn&& f) const &&
    {
        using U = detail::result0Transform<Fn>;
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

    /** Transform the stored error value when disengaged.
        @param f Transformation to apply.
        @return Expected containing the transformed error or the current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) &
    {
        using G = detail::ResultTransform<Fn, E&>;
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

    /** Transform the stored error value when disengaged (const lvalue).
        @param f Transformation to apply.
        @return Expected containing the transformed error or the current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) const &
    {
        using G = detail::ResultTransform<Fn, E const&>;
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

    /** Transform the stored error value when disengaged (rvalue).
        @param f Transformation to apply.
        @return Expected containing the transformed error or the current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) &&
    {
        using G = detail::ResultTransform<Fn, E&&>;
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

    /** Transform the stored error value when disengaged (const rvalue).
        @param f Transformation to apply.
        @return Expected containing the transformed error or the current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) const &&
    {
        using G = detail::ResultTransform<Fn, E const&&>;
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

/** Expected specialization for lvalue references.
    Holds either a bound reference to `T` or an unexpected error `E`.
*/
template <class T, class E>
class Expected<T&, E> {
    static_assert(detail::can_beUnexpected<E>);

    // Storage: either a bound pointer to T, or an error E.
    union {
        /** Pointer to the referenced value when engaged.
        */
        T* p_;
        /** Stored unexpected error when disengaged.
        */
        E unex_;
    };
    /** True when the Expected currently binds a reference.
    */
    bool has_value_ = false;

    // Short aliases
    using R = T&;
    template <class U>
    static constexpr bool ok_bind_v = detail::ok_bind_ref_v<R, U>;

public:
    /** Referenced value type.
    */
    using value_type = T&;
    /** Error type carried when disengaged.
    */
    using error_type = E;
    /** Convenience alias for the unexpected wrapper.
    */
    using unexpected_type = Unexpected<E>;

    /** Rebind the reference to another value type while keeping `E`.
    */
    template <class U>
    using rebind = Expected<U, error_type>;

    // ----------------------------------
    // ctors
    // ----------------------------------

    // Disengaged by default
    /** Construct a disengaged Expected with no bound reference.
    */
    constexpr
    Expected() noexcept
        : p_(nullptr), has_value_(false) {}

    // Success from lvalue: bind
    /** Bind to an lvalue result, marking the expected engaged.
    */
    template <class U>
    requires(!std::is_same_v<std::remove_cvref_t<U>, Expected>
             && !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
             && !detail::isUnexpected<std::remove_cvref_t<U>> && ok_bind_v<U&>)
    constexpr
    explicit(!std::is_convertible_v<U&, R>)
        Expected(U& u) noexcept(std::is_nothrow_constructible_v<R, U&>)
        : p_(std::addressof(static_cast<R>(u)))
        , has_value_(true)
    {}

    // Deleted when binding would be from a temporary / disallowed
    /** Deleted: binding a reference Expected from a temporary would dangle.
        @param u Temporary value (deleted).
    */
    template <class U>
    requires(!std::is_same_v<std::remove_cvref_t<U>, Expected>
             && !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
             && !detail::isUnexpected<std::remove_cvref_t<U>>
             && !ok_bind_v<U &&>)
    constexpr Expected(U&& u) = delete;

    // In-place: bind to an lvalue argument
    /** Bind in-place to an existing lvalue.
        @param tag Tag selecting in-place construction.
        @param u Lvalue to bind this Expected to.
    */
    template <class U>
    requires ok_bind_v<U&>
    constexpr
    explicit
    Expected(std::in_place_t tag, U& u) noexcept
        : p_(std::addressof(static_cast<R>(u)))
        , has_value_(true)
    {}

    // In-place via invocation result (mirrors your in_place_inv)
    /** Invoke a factory to obtain the bound reference in-place.
        @param fn Callable that returns a bindable lvalue.
    */
    template <class Fn>
    explicit
    constexpr
    Expected(detail::in_place_inv tag, Fn&& fn)
    {
        // Expect fn() to yield something bindable to R (i.e. an lvalue of T)
        auto&& r = std::forward<Fn>(fn)();
        static_assert(ok_bind_v<decltype(r)>);
        p_ = std::addressof(static_cast<R>(r));
        has_value_ = true;
    }

    // Error ctors (same rules as primary)
    /** Construct from an unexpected error (copy).
        @param u Unexpected wrapper to copy.
    */
    template <class G = E>
    requires std::is_constructible_v<E, G const&>
    constexpr
    explicit(!std::is_convertible_v<G const&, E>)
    Expected(Unexpected<G> const& u)
        noexcept(std::is_nothrow_constructible_v<E, G const&>)
        : unex_(u.error())
        , has_value_(false)
    {}

    /** Construct from an unexpected error (move).
        @param u Unexpected wrapper to move from.
    */
    template <class G = E>
    requires std::is_constructible_v<E, G>
    constexpr
    explicit(!std::is_convertible_v<G, E>)
    Expected(Unexpected<G>&& u)
        noexcept(std::is_nothrow_constructible_v<E, G>)
        : unex_(std::move(u).error())
        , has_value_(false)
    {}

    // Error via invocation-result (mirrors your unexpect_inv)
    /** Construct an unexpected state by invoking a factory.
        @param fn Callable that produces an error value.
    */
    template <class Fn>
    explicit
    constexpr
    Expected(detail::unexpect_inv tag, Fn&& fn)
        : unex_(std::forward<Fn>(fn)())
        , has_value_(false)
    {}

    // Converting ctors from other Expected -----------------------

    // From Expected<U&, E>: safe to bind for any value category
    /** Copy-construct from another reference Expected.
        @param other Source instance to bind to.
    */
    template <class U>
    requires detail::ok_bind_ref_v<R, U&>
    constexpr
    explicit(!std::is_convertible_v<U&, R>)
    Expected(Expected<U&, E> const& other)
        noexcept(std::is_nothrow_constructible_v<R, U&>
            && std::is_nothrow_copy_constructible_v<E>)
        : has_value_(other.has_value_)
    {
        if (has_value_)
        {
            p_ = std::addressof(static_cast<U&>(other.value()));
        } else
        {
            std::construct_at(std::addressof(unex_), other.error());
        }
    }

    /** Move-construct from another reference Expected.
        @param other Source instance to bind to.
    */
    template <class U>
    requires detail::ok_bind_ref_v<R, U&>
    constexpr
    explicit(!std::is_convertible_v<U&, R>)
    Expected(Expected<U&, E>&& other)
        noexcept(std::is_nothrow_constructible_v<R, U&>
            && std::is_nothrow_move_constructible_v<E>)
        : has_value_(other.has_value_)
    {
        if (has_value_)
        {
            p_ = std::addressof(static_cast<U&>(other.value()));
        } else
        {
            std::construct_at(std::addressof(unex_), std::move(other).error());
        }
    }

    // From Expected<U, E> (non-ref): only from lvalue object; forbid rvalue
    /** Bind to the result stored inside another Expected value.
        @param other Source instance providing the lvalue.
    */
    template <class U>
    requires detail::ok_bind_ref_v<R, U&>
    constexpr
    explicit(!std::is_convertible_v<U&, R>)
    Expected(Expected<U, E>& other)
        noexcept(std::is_nothrow_constructible_v<R, U&>
            && std::is_nothrow_copy_constructible_v<E>)
        : has_value_(other.has_value())
    {
        if (has_value_)
        {
            p_ = std::addressof(other.value());
        } else
        {
            std::construct_at(std::addressof(unex_), other.error());
        }
    }

    /** Deleted: rebinding from a temporary Expected<U, E> would dangle.
    */
    template <class U>
    constexpr
    Expected(Expected<U, E>&&) = delete; // would dangle

    // Copy/move/dtor
    /** Copy-construct from another reference Expected.
    */
    constexpr
    Expected(Expected const&) = default;

    /** Move-construct from another reference Expected.
    */
    constexpr
    Expected(Expected&&) = default;

    /** Construct a disengaged Expected from error arguments.
        @param tag Tag selecting unexpected construction.
        @param args Arguments forwarded to the error constructor.
    */
    template <class... Args>
    requires std::is_constructible_v<E, Args...>
    constexpr
    explicit
    Expected(unexpect_t tag, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : unex_(std::forward<Args>(args)...)
        , has_value_(false)
    {}

    /** Construct a disengaged Expected from an initializer list.
        @param tag Tag selecting unexpected construction.
        @param il Initializer list forwarded to the error constructor.
        @param args Additional arguments forwarded to the error constructor.
    */
    template <class U, class... Args>
    requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
    constexpr
    explicit
    Expected(unexpect_t tag, std::initializer_list<U> il, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U>&, Args...>)
        : unex_(il, std::forward<Args>(args)...)
        , has_value_(false)
    {}

    /** Destroy the held error when disengaged.
    */
    constexpr
    ~Expected()
    {
        if (!has_value_)
        {
            std::destroy_at(std::addressof(unex_));
        }
    }

    // ----------------------------------
    // assignment (always rebind)
    // ----------------------------------

    /** Copy-assign, rebinding or copying the error.
    */
    constexpr
    Expected&
    operator=(Expected const&) = default;

    /** Move-assign, rebinding or moving the error.
    */
    constexpr
    Expected&
    operator=(Expected&&) = default;

    // Assign from lvalue -> rebind
    /** Assign a new binding from an lvalue.
    */
    template <class U>
    requires ok_bind_v<U&>
    constexpr
    Expected&
    operator=(U& u)
        noexcept(std::is_nothrow_constructible_v<R, U&>)
    {
        if (!has_value_)
        {
            std::destroy_at(std::addressof(unex_));
            has_value_ = true;
        }
        p_ = std::addressof(static_cast<R>(u));
        return *this;
    }

    // Deleted for temporaries
    /** Deleted: rebinding from a temporary would dangle.
        @param u Temporary value (deleted).
    */
    template <class U>
    requires(!ok_bind_v<U &&>)
    constexpr Expected& operator=(U&& u) = delete;

    // Assign from Expected<U&,E> -> rebind or store error
    /** Assign from another reference Expected (copy).
        @param other Source instance to bind to or copy error from.
    */
    template <class U>
    requires detail::ok_bind_ref_v<R, U&>
    constexpr
    Expected&
    operator=(Expected<U&, E> const& other)
    {
        if (other.has_value())
        {
            if (!has_value_)
            {
                std::destroy_at(std::addressof(unex_));
                has_value_ = true;
            }
            p_ = std::addressof(static_cast<U&>(other.value()));
        } else
        {
            if (has_value_)
            {
                std::construct_at(std::addressof(unex_), other.error());
                has_value_ = false;
            } else
            {
                unex_ = other.error();
            }
        }
        return *this;
    }

    /** Assign from another reference Expected (move).
        @param other Source instance to bind to or move error from.
    */
    template <class U>
    requires detail::ok_bind_ref_v<R, U&>
    constexpr
    Expected&
    operator=(Expected<U&, E>&& other)
    {
        if (other.has_value())
        {
            if (!has_value_)
            {
                std::destroy_at(std::addressof(unex_));
                has_value_ = true;
            }
            p_ = std::addressof(static_cast<U&>(other.value()));
        } else
        {
            if (has_value_)
            {
                std::construct_at(
                    std::addressof(unex_),
                    std::move(other).error());
                has_value_ = false;
            } else
            {
                unex_ = std::move(other).error();
            }
        }
        return *this;
    }

    // Assign from Expected<U,E> lvalue only (non-ref). Rvalue deleted to avoid
    // dangling.
    /** Bind to the value contained in another Expected instance.
        @param other Source lvalue Expected to bind to.
    */
    template <class U>
    requires detail::ok_bind_ref_v<R, U&>
    constexpr
    Expected&
    operator=(Expected<U, E>& other)
    {
        if (other.has_value())
        {
            if (!has_value_)
            {
                std::destroy_at(std::addressof(unex_));
                has_value_ = true;
            }
            p_ = std::addressof(other.value());
        } else
        {
            if (has_value_)
            {
                std::construct_at(std::addressof(unex_), other.error());
                has_value_ = false;
            } else
            {
                unex_ = other.error();
            }
        }
        return *this;
    }

    /** Deleted: cannot bind to a temporary Expected holding a value.
    */
    template <class U>
    constexpr
    Expected&
    operator=(Expected<U, E>&&) = delete;

    // Assign error
    /** Replace the stored state with an unexpected error (copy).
        @param e Unexpected wrapper to copy from.
    */
    template <class G>
    requires std::is_constructible_v<E, G const&>
             && std::is_assignable_v<E&, G const&>
    constexpr
    Expected&
    operator=(Unexpected<G> const& e)
    {
        if (has_value_)
        {
            std::construct_at(std::addressof(unex_), e.error());
            has_value_ = false;
        } else
        {
            unex_ = e.error();
        }
        return *this;
    }

    /** Replace the stored state with an unexpected error (move).
        @param e Unexpected wrapper to move from.
    */
    template <class G>
    requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
    constexpr
    Expected&
    operator=(Unexpected<G>&& e)
    {
        if (has_value_)
        {
            std::construct_at(std::addressof(unex_), std::move(e).error());
            has_value_ = false;
        } else
        {
            unex_ = std::move(e).error();
        }
        return *this;
    }

    // Emplace: bind to an lvalue
    /** Rebind to a new lvalue, returning the stored reference.
        @param u Reference to bind to.
        @return Bound reference.
    */
    template <class U>
    requires ok_bind_v<U&>
    constexpr
    T&
    emplace(U& u) noexcept
    {
        if (!has_value_)
        {
            std::destroy_at(std::addressof(unex_));
            has_value_ = true;
        }
        p_ = std::addressof(static_cast<R>(u));
        return *p_;
    }

    /** Deleted: cannot bind reference Expected to a temporary.
    */
    template <class U>
    requires(!ok_bind_v<U &&>)
    constexpr T& emplace(U&& u) = delete;

    // swap
    /** Swap state with another reference Expected.
        @param x Other instance to exchange with.
    */
    constexpr
    void
    swap(Expected& x)
        noexcept(
        std::is_nothrow_move_constructible_v<E>
        && std::is_nothrow_swappable_v<E&>)
    requires std::is_swappable_v<E>
    {
        if (has_value_)
        {
            if (x.has_value_)
            {
                using std::swap;
                swap(p_, x.p_);
            } else
            {
                // this has value, x has error
                E tmp(std::move(x.unex_));
                std::destroy_at(std::addressof(x.unex_));
                x.p_ = p_;
                x.has_value_ = true;

                std::construct_at(std::addressof(unex_), std::move(tmp));
                has_value_ = false;
            }
        } else
        {
            if (x.has_value_)
            {
                x.swap(*this);
            } else
            {
                using std::swap;
                swap(unex_, x.unex_);
            }
        }
    }

    friend constexpr
    void
    swap(Expected& a, Expected& b)
        noexcept(noexcept(a.swap(b)))
    requires requires { a.swap(b); }
    {
        a.swap(b);
    }

    // ----------------------------------
    // observers
    // ----------------------------------
    /** Return true when a reference is bound.
    */
    [[nodiscard]]
    constexpr
    explicit
    operator bool() const noexcept
    {
        return has_value_;
    }

    /** Check whether the Expected currently contains a reference.
        @return `true` if a reference is bound.
    */
    [[nodiscard]]
    constexpr
    bool
    has_value() const noexcept
    {
        return has_value_;
    }

    /** Access the bound reference pointer; undefined if disengaged.
        @return Pointer to the bound value.
    */
    [[nodiscard]]
    constexpr
    T*
    operator->() noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return p_;
    }

    /** Access the bound reference pointer; undefined if disengaged.
        @return Pointer to the bound value.
    */
    [[nodiscard]]
    constexpr
    T const*
    operator->() const noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return p_;
    }

    /** Dereference the bound reference.
        @return Reference to the bound value.
    */
    [[nodiscard]]
    constexpr
    T&
    operator*() & noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return *p_;
    }

    /** Dereference the bound reference (const).
        @return Reference to the bound value.
    */
    [[nodiscard]]
    constexpr
    T const&
    operator*() const& noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return *p_;
    }

    /** Dereference the bound reference, preserving value category.
        @return Reference to the bound value.
    */
    [[nodiscard]]
    constexpr
    T&&
    operator*() && noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::move(*p_);
    }

    /** Dereference the bound reference, preserving value category (const rvalue).
        @return Reference to the bound value.
    */
    [[nodiscard]]
    constexpr
    T const&&
    operator*() const&& noexcept
    {
        MRDOCS_ASSERT(has_value_);
        return std::move(*p_);
    }

    /** Return the bound reference or throw on disengaged state.
    */
    constexpr
    T&
    value() &
    {
        if (has_value_)
        {
            return *p_;
        }
        throw BadExpectedAccess<E>(error());
    }

    /** Return the bound reference or throw on disengaged state (const lvalue).
    */
    constexpr
    T const&
    value() const&
    {
        if (has_value_)
        {
            return *p_;
        }
        throw BadExpectedAccess<E>(error());
    }

    /** Return the bound reference or throw on disengaged state (rvalue).
    */
    constexpr
    T&&
    value() &&
    {
        if (has_value_)
        {
            return std::move(*p_);
        }
        throw BadExpectedAccess<E>(std::move(error()));
    }

    /** Return the bound reference or throw on disengaged state (const rvalue).
    */
    constexpr
    T const&&
    value() const&&
    {
        if (has_value_)
        {
            return std::move(*p_);
        }
        throw BadExpectedAccess<E>(std::move(error()));
    }

    /** Access the stored error; requires disengaged state.
        @return Reference to the stored error.
    */
    [[nodiscard]]
    constexpr
    E&
    error() & noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    /** Access the stored error (const lvalue).
        @return Reference to the stored error.
    */
    [[nodiscard]]
    constexpr
    E const&
    error() const& noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return unex_;
    }

    /** Access the stored error (rvalue).
        @return Rvalue reference to the stored error.
    */
    [[nodiscard]]
    constexpr
    E&&
    error() && noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    /** Access the stored error (const rvalue).
        @return Rvalue reference to the stored error.
    */
    [[nodiscard]]
    constexpr
    E const&&
    error() const&& noexcept
    {
        MRDOCS_ASSERT(!has_value_);
        return std::move(unex_);
    }

    // value_or: return by value (copy/move), like your non-ref primary
    /** Return the bound value or a fallback copy when disengaged.
        @param u Fallback value to return if no reference is bound.
        @return Bound value or fallback copy.
    */
    template <class U>
    constexpr
    std::remove_reference_t<T>
    value_or(U&& u) const&
    {
        using Rval = std::remove_reference_t<T>;
        static_assert(std::is_copy_constructible_v<Rval>);
        static_assert(std::is_convertible_v<U, Rval>);
        return has_value_ ? *p_ : static_cast<Rval>(std::forward<U>(u));
    }

    /** Return the bound value or a fallback move when disengaged.
        @param u Fallback value to move if no reference is bound.
        @return Bound value or fallback.
    */
    template <class U>
    constexpr
    std::remove_reference_t<T>
    value_or(U&& u) &&
    {
        using Rval = std::remove_reference_t<T>;
        static_assert(std::is_move_constructible_v<Rval>);
        static_assert(std::is_convertible_v<U, Rval>);
        return has_value_ ? std::move(*p_) :
                            static_cast<Rval>(std::forward<U>(u));
    }

    // error_or: identical to primary
    /** Return the stored error or the provided fallback (copy).
        @param g Fallback error to return when engaged.
        @return Error value.
    */
    template <class G = E>
    constexpr
    E
    error_or(G&& g) const&
    {
        return has_value_ ? static_cast<E>(std::forward<G>(g)) : unex_;
    }

    /** Return the stored error or the provided fallback (move).
        @param g Fallback error to move when engaged.
        @return Error value.
    */
    template <class G = E>
    constexpr
    E
    error_or(G&& g) &&
    {
        return has_value_ ? static_cast<E>(std::forward<G>(g)) :
                            std::move(unex_);
    }

    // ----------------------------------
    // monadic ops
    // ----------------------------------

    // and_then: F(T&) -> Expected<*, E>
    /** Invoke `f` with the bound reference when engaged.
        @param f Continuation producing another Expected.
        @return Result of `f` or current error.
    */
    template <class Fn>
    constexpr
    auto
    and_then(Fn&& f) &
    {
        using U = std::remove_cvref_t<std::invoke_result_t<Fn, T&>>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);
        if (has_value_)
        {
            return std::invoke(std::forward<Fn>(f), *p_);
        }
        return U(unexpect, unex_);
    }

    /** Invoke `f` with the bound reference when engaged (const lvalue).
        @param f Continuation producing another Expected.
        @return Result of `f` or current error.
    */
    template <class Fn>
    constexpr
    auto
    and_then(Fn&& f) const&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<Fn, T const&>>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);
        if (has_value_)
        {
            return std::invoke(std::forward<Fn>(f), *p_);
        }
        return U(unexpect, unex_);
    }

    /** Invoke `f` with the bound reference when engaged (rvalue).
        @param f Continuation producing another Expected.
        @return Result of `f` or current error.
    */
    template <class Fn>
    constexpr
    auto
    and_then(Fn&& f) &&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<Fn, T&&>>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);
        if (has_value_)
        {
            return std::invoke(std::forward<Fn>(f), std::move(*p_));
        }
        return U(unexpect, std::move(unex_));
    }

    /** Invoke `f` with the bound reference when engaged (const rvalue).
        @param f Continuation producing another Expected.
        @return Result of `f` or current error.
    */
    template <class Fn>
    constexpr
    auto
    and_then(Fn&& f) const&&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<Fn, T const&&>>;
        static_assert(detail::isExpected<U>);
        static_assert(std::is_same_v<typename U::error_type, E>);
        if (has_value_)
        {
            return std::invoke(std::forward<Fn>(f), std::move(*p_));
        }
        return U(unexpect, std::move(unex_));
    }

    // or_else: same signature/behavior as primary; when engaged, return
    // self-type with same binding
    /** Recover with `f` if disengaged, otherwise return this value.
        @param f Recovery function returning an Expected.
    */
    template <class Fn>
    constexpr
    Expected
    or_else(Fn&& f) &
    {
        if (has_value_)
        {
            return *this;
        }
        auto g = std::forward<Fn>(f)(unex_);
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(g)>, Expected>);
        return g;
    }

    /** Recover with `f` if disengaged (const lvalue).
        @param f Recovery function returning an Expected.
    */
    template <class Fn>
    constexpr
    Expected
    or_else(Fn&& f) const&
    {
        if (has_value_)
        {
            return *this;
        }
        auto g = std::forward<Fn>(f)(unex_);
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(g)>, Expected>);
        return g;
    }

    /** Recover with `f` if disengaged (rvalue).
        @param f Recovery function returning an Expected.
    */
    template <class Fn>
    constexpr
    Expected
    or_else(Fn&& f) &&
    {
        if (has_value_)
        {
            return *this;
        }
        auto g = std::forward<Fn>(f)(std::move(unex_));
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(g)>, Expected>);
        return g;
    }

    /** Recover with `f` if disengaged (const rvalue).
        @param f Recovery function returning an Expected.
    */
    template <class Fn>
    constexpr
    Expected
    or_else(Fn&& f) const&&
    {
        if (has_value_)
        {
            return *this;
        }
        auto g = std::forward<Fn>(f)(std::move(unex_));
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(g)>, Expected>);
        return g;
    }

    // transform: F(T&) -> U ; returns Expected<U, E>
    /** Transform the bound value when engaged.
        @param f Transformation applied to the bound reference.
        @return Expected containing transformed value or current error.
    */
    template <class Fn>
    constexpr
    auto
    transform(Fn&& f) &
    {
        using U = std::remove_cv_t<std::invoke_result_t<Fn, T&>>;
        using Res = Expected<U, E>;
        if (has_value_)
        {
            return Res(detail::in_place_inv{}, [&] {
                return std::invoke(std::forward<Fn>(f), *p_);
            });
        }
        return Res(unexpect, unex_);
    }

    /** Transform the bound value when engaged (const lvalue).
        @param f Transformation applied to the bound reference.
        @return Expected containing transformed value or current error.
    */
    template <class Fn>
    constexpr
    auto
    transform(Fn&& f) const&
    {
        using U = std::remove_cv_t<std::invoke_result_t<Fn, T const&>>;
        using Res = Expected<U, E>;
        if (has_value_)
        {
            return Res(detail::in_place_inv{}, [&] {
                return std::invoke(std::forward<Fn>(f), *p_);
            });
        }
        return Res(unexpect, unex_);
    }

    /** Transform the bound value when engaged (rvalue).
        @param f Transformation applied to the bound reference.
        @return Expected containing transformed value or current error.
    */
    template <class Fn>
    constexpr
    auto
    transform(Fn&& f) &&
    {
        using U = std::remove_cv_t<std::invoke_result_t<Fn, T&&>>;
        using Res = Expected<U, E>;
        if (has_value_)
        {
            return Res(detail::in_place_inv{}, [&] {
                return std::invoke(std::forward<Fn>(f), std::move(*p_));
            });
        }
        return Res(unexpect, std::move(unex_));
    }

    /** Transform the bound value when engaged (const rvalue).
        @param f Transformation applied to the bound reference.
        @return Expected containing transformed value or current error.
    */
    template <class Fn>
    constexpr
    auto
    transform(Fn&& f) const&&
    {
        using U = std::remove_cv_t<std::invoke_result_t<Fn, T const&&>>;
        using Res = Expected<U, E>;
        if (has_value_)
        {
            return Res(detail::in_place_inv{}, [&] {
                return std::invoke(std::forward<Fn>(f), std::move(*p_));
            });
        }
        return Res(unexpect, std::move(unex_));
    }

    // transform_error: identical to primary
    /** Transform the stored error when disengaged.
        @param f Transformation applied to the error.
        @return Expected with transformed error or current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) &
    {
        using G = std::remove_cv_t<std::invoke_result_t<Fn, E&>>;
        using Res = Expected<T&, G>;
        if (has_value_)
        {
            return Res(std::in_place, *p_);
        }
        return Res(detail::unexpect_inv{}, [&] {
            return std::invoke(std::forward<Fn>(f), unex_);
        });
    }

    /** Transform the stored error when disengaged (const lvalue).
        @param f Transformation applied to the error.
        @return Expected with transformed error or current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) const&
    {
        using G = std::remove_cv_t<std::invoke_result_t<Fn, E const&>>;
        using Res = Expected<T&, G>;
        if (has_value_)
        {
            return Res(std::in_place, *p_);
        }
        return Res(detail::unexpect_inv{}, [&] {
            return std::invoke(std::forward<Fn>(f), unex_);
        });
    }

    /** Transform the stored error when disengaged (rvalue).
        @param f Transformation applied to the error.
        @return Expected with transformed error or current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) &&
    {
        using G = std::remove_cv_t<std::invoke_result_t<Fn, E&&>>;
        using Res = Expected<T&, G>;
        if (has_value_)
        {
            return Res(std::in_place, *p_);
        }
        return Res(detail::unexpect_inv{}, [&] {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        });
    }

    /** Transform the stored error when disengaged (const rvalue).
        @param f Transformation applied to the error.
        @return Expected with transformed error or current value.
    */
    template <class Fn>
    constexpr
    auto
    transform_error(Fn&& f) const&&
    {
        using G = std::remove_cv_t<std::invoke_result_t<Fn, E const&&>>;
        using Res = Expected<T&, G>;
        if (has_value_)
        {
            return Res(std::in_place, *p_);
        }
        return Res(detail::unexpect_inv{}, [&] {
            return std::invoke(std::forward<Fn>(f), std::move(unex_));
        });
    }

    template <class U, class E2>
    friend
    constexpr
    bool
    operator==(Expected const& x, Expected<U, E2> const& y)
        noexcept(noexcept(bool(*x == *y)) &&
                 noexcept(bool(x.error() == y.error())))
    requires (!std::is_void_v<U>)
    {
        if (x.has_value())
        {
            return y.has_value() && bool(*x == *y);
        } else
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
    operator==(Expected const& x, Unexpected<E2> const& e)
        noexcept(noexcept(bool(x.error() == e.error())))
    {
        return !x.has_value() && bool(x.error() == e.error());
    }
}; // class Expected<T&, E>


} // mrdocs

#endif
