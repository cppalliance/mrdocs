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

template <class T>
class Optional;

namespace detail {
template <typename T, typename U>
using ConvertsFromAnyCvRef = std::disjunction<
    std::is_constructible<T, U&>,
    std::is_convertible<U&, T>,
    std::is_constructible<T, U>,
    std::is_convertible<U, T>,
    std::is_constructible<T, U const&>,
    std::is_convertible<U const&, T>,
    std::is_constructible<T, U const>,
    std::is_convertible<U const, T>>;

template <typename T, typename U>
using ConvertsFromOptional = ConvertsFromAnyCvRef<T, Optional<U>>;

template <typename T, typename U>
using AssignsFromOptional = std::disjunction<
    std::is_assignable<T&, Optional<U> const&>,
    std::is_assignable<T&, Optional<U>&>,
    std::is_assignable<T&, Optional<U> const&&>,
    std::is_assignable<T&, Optional<U>&&>>;

template<typename T>
inline constexpr bool isOptionalV = false;

template<typename T>
inline constexpr bool isOptionalV<Optional<T>> = true;

template<typename T>
inline constexpr bool isOptionalV<Optional<T&>> = true;
}

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

    template<typename From, typename = std::remove_cv_t<T>>
    static constexpr bool NotConstructingBoolFromOptional
        = true;

    template <typename From>
    static constexpr bool NotConstructingBoolFromOptional<From, bool>
        = !detail::isOptionalV<std::remove_cvref_t<From>>;

    template<typename From, typename = std::remove_cv_t<T>>
    static constexpr bool ConstructFromContainedValue
        = !detail::ConvertsFromOptional<T, From>::value;

    template<typename From>
    static constexpr bool ConstructFromContainedValue<From, bool>
        = true;


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

    /** Construct from a value.

        @param u The value to store. It must be convertible to T.
     **/
    template <typename U = std::remove_cv_t<T>>
    requires(!std::is_same_v<Optional, std::remove_cvref_t<U>>)
            && (!std::is_same_v<std::in_place_t, std::remove_cvref_t<U>>)
            && std::is_constructible_v<T, U>
            && NotConstructingBoolFromOptional<U>
    constexpr explicit(!std::is_convertible_v<U, T>)
    Optional(U&& u) noexcept(std::is_nothrow_constructible_v<T, U>)
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

    template <typename U>
    requires(!std::is_same_v<T, U>) && std::is_constructible_v<T, U const&>
            && ConstructFromContainedValue<U>
    constexpr explicit(!std::is_convertible_v<U const&, T>)
    Optional(Optional<U> const& t) noexcept(std::is_nothrow_constructible_v<T, U const&>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                if (t)
                    return storage_t(static_cast<T>(*t));
                else
                    return storage_t(nullable_traits<T>::null());
            } else
            {
                if (t)
                    return storage_t(*t);
                else
                    return storage_t(std::nullopt);
            }
        }())
    {}

    template <typename U>
    requires(!std::is_same_v<T, U>)
            && std::is_constructible_v<T, U> && ConstructFromContainedValue<U>
    constexpr explicit(!std::is_convertible_v<U, T>)
    Optional(Optional<U>&& t) noexcept(std::is_nothrow_constructible_v<T, U>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                if (t)
                    return storage_t(static_cast<T>(std::move(*t)));
                else
                    return storage_t(nullable_traits<T>::null());
            } else
            {
                if (t)
                    return storage_t(std::move(*t));
                else
                    return storage_t(std::nullopt);
            }
        }())
    {}

    template <typename U>
    requires
        std::is_constructible_v<T, U const&> &&
        ConstructFromContainedValue<U>
    constexpr explicit(!std::is_convertible_v<U const&, T>)
    Optional(std::optional<U> const& t) noexcept(std::is_nothrow_constructible_v<T, U const&>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                if (t)
                    return storage_t(static_cast<T>(*t));
                else
                    return storage_t(nullable_traits<T>::null());
            } else
            {
                if (t)
                    return storage_t(*t);
                else
                    return storage_t(std::nullopt);
            }
        }())
    {}

    template <typename U>
    requires
        std::is_constructible_v<T, U> &&
        ConstructFromContainedValue<U>
    constexpr explicit(!std::is_convertible_v<U, T>)
    Optional(std::optional<U>&& t) noexcept(std::is_nothrow_constructible_v<T, U>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                if (t)
                    return storage_t(static_cast<T>(std::move(*t)));
                else
                    return storage_t(nullable_traits<T>::null());
            } else
            {
                if (t)
                    return storage_t(std::move(*t));
                else
                    return storage_t(std::nullopt);
            }
        }())
    {}

    template <typename... Args>
    requires std::is_constructible_v<T, Args...>
    explicit constexpr
    Optional(std::in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                return storage_t(T(std::forward<Args>(args)...));
            } else
            {
                return storage_t(std::in_place, std::forward<Args>(args)...);
            }
        }())
    {}

    template <typename U, typename... Args>
    requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
    explicit constexpr Optional(
        std::in_place_t, std::initializer_list<U> il, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                return storage_t(T(il, std::forward<Args>(args)...));
            } else
            {
                return storage_t(std::in_place, il, std::forward<Args>(args)...);
            }
        }())
    {}

    /// Copy assignment
    constexpr Optional&
    operator=(Optional const&) = default;

    /// Move assignment
    constexpr Optional&
    operator=(Optional&&) = default;

    constexpr Optional&
    operator=(std::nullptr_t) noexcept(reset_noex_())
    {
        reset();
        MRDOCS_ASSERT(!has_value());
        return *this;
    }

    /** Assign from a value.

        @param u The value to store. It must be convertible to T.
     **/
    template <typename U = std::remove_cv_t<T>>
    requires(!std::is_same_v<Optional, std::remove_cvref_t<U>>)
            && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>
    constexpr Optional&
    operator=(U&& u) noexcept(
        std::is_nothrow_constructible_v<T, U> &&
        std::is_nothrow_assignable_v <T&, U>)
    {
        if constexpr (uses_nullable_traits)
        {
            s_ = std::forward<U>(u);
        }
        else
        {
            s_ = std::forward<U>(u);
        }
        return *this;
    }

    template<typename U>
    requires (!std::is_same_v<T, U>)
            && std::is_constructible_v<T, const U&>
            && std::is_assignable_v<T&, const U&>
            && (!detail::ConvertsFromOptional<T, U>::value)
            && (!detail::AssignsFromOptional<T, U>::value)
    constexpr Optional&
    operator=(const Optional<U>& u) noexcept(
        std::is_nothrow_constructible_v<T, U const&>
        && std::is_nothrow_assignable_v<T&, U const&>)
    {
        if (u)
        {
            if constexpr (uses_nullable_traits)
            {
                s_ = *u;
            }
            else
            {
                s_ = *u;
            }
        }
        else
        {
            reset();
        }
        return *this;
    }


    template<typename U>
    requires (!std::is_same_v<T, U>)
            && std::is_constructible_v<T, U>
            && std::is_assignable_v<T&, U>
            && (!detail::ConvertsFromOptional<T, U>::value)
            && (!detail::AssignsFromOptional<T, U>::value)
    constexpr Optional&
    operator=(Optional<U>&& u) noexcept(
        std::is_nothrow_constructible_v<T, U>
        && std::is_nothrow_assignable_v<T&, U>)
    {
        if (u)
        {
            if constexpr (uses_nullable_traits)
            {
                s_ = std::move(*u);
            }
            else
            {
                s_ = std::move(*u);
            }
        }
        else
        {
            reset();
        }
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

    /** Determine if the value is inlined via nullable traits.

        This is a compile-time property of T. If nullable_traits<T> is not
        specialized, this function returns false to indicate that the
        optional uses std::optional<T> as storage with an extra discriminator.
        If nullable_traits<T> is specialized, this function returns true
        to suggest that the null state is encoded inside T and no extra
        storage is used.

        @return `true` if the optional uses nullable_traits<T> for storage.
     */
    static constexpr bool
    is_inlined() noexcept
    {
        return uses_nullable_traits;
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
template <typename T>
using OptionalRelOpT = std::enable_if_t<std::is_convertible_v<T, bool>, bool>;

template <typename T, typename U>
using OptionalEqT = OptionalRelOpT<
    decltype(std::declval<T const&>() == std::declval<U const&>())>;

template <typename T, typename U>
using OptionalNeT = OptionalRelOpT<
    decltype(std::declval<T const&>() != std::declval<U const&>())>;

template <typename T, typename U>
using OptionalLtT = OptionalRelOpT<
    decltype(std::declval<T const&>() < std::declval<U const&>())>;

template <typename T, typename U>
using OptionalGtT = OptionalRelOpT<
    decltype(std::declval<T const&>() > std::declval<U const&>())>;

template <typename T, typename U>
using OptionalLeT = OptionalRelOpT<
    decltype(std::declval<T const&>() <= std::declval<U const&>())>;

template <typename T, typename U>
using OptionalGeT = detail::OptionalRelOpT<
    decltype(std::declval<T const&>() >= std::declval<U const&>())>;

template <typename T>
concept isDerivedFromOptional = requires(T const& t) {
    []<typename U>(Optional<U> const&) {
    }(t);
};

} // namespace detail


namespace detail {
#if defined(__cpp_lib_reference_from_temporary)
using std::reference_constructs_from_temporary_v;
using std::reference_converts_from_temporary_v;
#else
template<class To, class From>
concept reference_converts_from_temporary_v =
    std::is_reference_v<To> &&
    (
        (!std::is_reference_v<From> &&
         std::is_convertible_v<std::remove_cvref_t<From>*,
                               std::remove_cvref_t<To>*>)
        ||
        (std::is_lvalue_reference_v<To> &&
         std::is_const_v<std::remove_reference_t<To>> &&
         std::is_convertible_v<From, const std::remove_cvref_t<To>&&> &&
         !std::is_convertible_v<From, std::remove_cvref_t<To>&>)
    );

template <class To, class From>
inline constexpr bool reference_constructs_from_temporary_v
    = reference_converts_from_temporary_v<To, From>;
#endif
} // detail

template <class T>
class Optional<T&> {
    T* p_ = nullptr;

    template <class U>
    static constexpr bool ok_bind_v
        = std::is_constructible_v<T&, U>
          && !detail::reference_constructs_from_temporary_v<T&, U>;

public:
    using value_type = T;

    constexpr
    Optional() noexcept = default;

    constexpr
    Optional(Optional const&) noexcept = default;

    constexpr
    Optional(Optional&&) noexcept = default;

    constexpr
    Optional(std::nullopt_t) noexcept
        : Optional() {}

    template <class U>
    requires(
        !std::is_same_v<std::remove_cvref_t<U>, Optional> &&
        !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
        ok_bind_v<U>)
    constexpr
    explicit(!std::is_convertible_v<U, T&>)
    Optional(U&& u)
        noexcept(std::is_nothrow_constructible_v<T&, U>)
    {
        T& r(static_cast<U&&>(u));
        p_ = std::addressof(r);
    }

    template <class U>
    requires ok_bind_v<U&>
    constexpr
    explicit(!std::is_convertible_v<U&, T&>)
    Optional(Optional<U>& rhs)
        noexcept(std::is_nothrow_constructible_v<T&, U&>)
    {
        if (rhs)
        {
            p_ = std::addressof(*rhs);
        }
    }

    template <class U>
    requires ok_bind_v<U const&>
    constexpr
    explicit(!std::is_convertible_v<U const&, T&>)
    Optional(Optional<U> const& rhs)
        noexcept(std::is_nothrow_constructible_v<T&, U const&>)
    {
        if (rhs)
        {
            p_ = std::addressof(*rhs);
        }
    }

    template <class U>
    requires ok_bind_v<U&>
    constexpr
    Optional(std::optional<U>& o)
        noexcept(std::is_nothrow_constructible_v<T&, U&>)
    {
        if (o)
        {
            p_ = std::addressof(*o);
        }
    }

    template <class U>
    requires ok_bind_v<U const&>
    constexpr
    Optional(std::optional<U> const& o)
        noexcept(std::is_nothrow_constructible_v<T&, U const&>)
    {
        if (o)
        {
            p_ = std::addressof(*o);
        }
    }

    constexpr Optional&
    operator=(Optional const&) noexcept = default;

    constexpr Optional&
    operator=(Optional&&) noexcept = default;

    constexpr Optional&
    operator=(std::nullopt_t) noexcept
    {
        p_ = nullptr;
        return *this;
    }

    template <class U>
    requires ok_bind_v<U>
    constexpr Optional&
    operator=(U&& u)
        noexcept(std::is_nothrow_constructible_v<T&, U>)
    {
        T& r(static_cast<U&&>(u));
        p_ = std::addressof(r);
        return *this;
    }

    template <class U>
    requires ok_bind_v<U&>
    constexpr
    Optional&
    operator=(Optional<U>& rhs)
        noexcept(std::is_nothrow_constructible_v<T&, U&>)
    {
        p_ = rhs ? std::addressof(*rhs) : nullptr;
        return *this;
    }

    template <class U>
    requires ok_bind_v<U const&>
    constexpr
    Optional&
    operator=(Optional<U> const& rhs)
        noexcept(std::is_nothrow_constructible_v<T&, U const&>)
    {
        p_ = rhs ? std::addressof(*rhs) : nullptr;
        return *this;
    }

    template <class U>
    requires ok_bind_v<U>
    constexpr
    Optional&
    operator=(Optional<U>&& rhs)
        noexcept(std::is_nothrow_constructible_v<T&, U>)
    {
        p_ = rhs ? std::addressof(*rhs) : nullptr;
        return *this;
    }

    template <class U>
    requires ok_bind_v<U>
    constexpr
    value_type&
    emplace(U&& u)
        noexcept(std::is_nothrow_constructible_v<T&, U>)
    {
        T& r(static_cast<U&&>(u));
        p_ = std::addressof(r);
        return *p_;
    }

    static
    constexpr
    bool
    is_inlined() noexcept
    {
        return true;
    }

    constexpr
    bool
    has_value() const noexcept
    {
        return p_ != nullptr;
    }

    constexpr
    explicit
    operator bool() const noexcept
    {
        return has_value();
    }

    constexpr
    void
    reset() noexcept
    {
        p_ = nullptr;
    }

    constexpr
    value_type*
    operator->() noexcept
    {
        MRDOCS_ASSERT(has_value());
        return p_;
    }

    constexpr
    value_type const*
    operator->() const noexcept
    {
        MRDOCS_ASSERT(has_value());
        return p_;
    }

    constexpr
    value_type&
    operator*() noexcept
    {
        MRDOCS_ASSERT(has_value());
        return *p_;
    }

    constexpr
    value_type const&
    operator*() const noexcept
    {
        MRDOCS_ASSERT(has_value());
        return *p_;
    }

    constexpr
    value_type&
    value() & noexcept
    {
        MRDOCS_ASSERT(has_value());
        return *p_;
    }

    constexpr
    value_type const&
    value() const& noexcept
    {
        MRDOCS_ASSERT(has_value());
        return *p_;
    }

    constexpr
    value_type&
    value() && noexcept
    {
        MRDOCS_ASSERT(has_value());
        return *p_;
    }

    constexpr
    value_type const&
    value() const&& noexcept
    {
        MRDOCS_ASSERT(has_value());
        return *p_;
    }

    constexpr void
    swap(Optional& other) noexcept
    {
        using std::swap;
        swap(p_, other.p_);
    }
};

template <class T>
constexpr void
swap(Optional<T&>& a, Optional<T&>& b) noexcept
{
    a.swap(b);
}

/** Compares two Optional values for equality.
    Returns true if both are engaged and their contained values are equal, or both are disengaged.
    @return `true` if both optionals are engaged and equal, or both are disengaged; otherwise, `false`.
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
    @return `true` if the optionals differ in engagement or value; otherwise, `false`.
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
    @return `true` if `lhs` is less than `rhs` according to the described rules; otherwise, `false`.
*/
template <typename T, typename U>
constexpr detail::OptionalLtT<T, U>
operator<(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return static_cast<bool>(rhs) && (!lhs || *lhs < *rhs);
}

/** Checks if the left Optional is greater than the right Optional.
    Returns true if the left is engaged and either the right is disengaged or its value is greater.
    @return `true` if `lhs` is greater than `rhs` according to the described rules; otherwise, `false`.
*/
template <typename T, typename U>
constexpr detail::OptionalGtT<T, U>
operator>(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return static_cast<bool>(lhs) && (!rhs || *lhs > *rhs);
}

/** Checks if the left Optional is less than or equal to the right Optional.
    Returns true if the left is disengaged or the right is engaged and the left's value is less or equal.
    @return `true` if `lhs` is less than or equal to `rhs` according to the described rules; otherwise, `false`.
*/
template <typename T, typename U>
constexpr detail::OptionalLeT<T, U>
operator<=(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return !lhs || (static_cast<bool>(rhs) && *lhs <= *rhs);
}

/** Checks if the left Optional is greater than or equal to the right Optional.
    Returns true if the right is disengaged or the left is engaged and its value is greater or equal.
    @return `true` if `lhs` is greater than or equal to `rhs` according to the described rules; otherwise, `false`.
*/
template <typename T, typename U>
constexpr detail::OptionalGeT<T, U>
operator>=(Optional<T> const& lhs, Optional<U> const& rhs)
{
    return !rhs || (static_cast<bool>(lhs) && *lhs >= *rhs);
}

/** Performs a three-way comparison between two Optional values.
    If both are engaged, compares their contained values; otherwise, compares engagement state.
    @return The result of the three-way comparison between the optionals or their values.
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
    @return `true` if the optional is disengaged; otherwise, `false`.
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
    @return The result of the three-way comparison with `std::nullopt`.
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
    @return `true` if the optional is engaged and equal to `rhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalEqT<T, U>
operator==(Optional<T> const& lhs, U const& rhs)
{
    return lhs && *lhs == rhs;
}

/** Compares a value to an engaged Optional for equality.
    Returns true if the Optional is engaged and its value equals lhs.
    @return `true` if the optional is engaged and equal to `lhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalEqT<T, U>
operator==(T const& lhs, Optional<U> const& rhs)
{
    return rhs && lhs == *rhs;
}

/** Compares an Optional to a value for inequality.
    Returns true if the Optional is disengaged or its value does not equal rhs.
    @return `true` if the optional is disengaged or not equal to `rhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalNeT<T, U>
operator!=(Optional<T> const& lhs, U const& rhs)
{
    return !lhs || *lhs != rhs;
}

/** Compares a value to an Optional for inequality.
    Returns true if the Optional is disengaged or its value does not equal lhs.
    @return `true` if the optional is disengaged or not equal to `lhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalNeT<T, U>
operator!=(T const& lhs, Optional<U> const& rhs)
{
    return !rhs || lhs != *rhs;
}

/** Checks if the Optional is less than a value.
    Returns true if the Optional is disengaged or its value is less than rhs.
    @return `true` if the optional is disengaged or less than `rhs`; otherwise, `false`.
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
    @return `true` if the optional is engaged and `lhs` is less than its value; otherwise, `false`.
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
    @return `true` if the optional is engaged and greater than `rhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalGtT<T, U>
operator>(Optional<T> const& lhs, U const& rhs)
{
    return lhs && *lhs > rhs;
}

/** Checks if a value is greater than an Optional.
    Returns true if the Optional is disengaged or lhs is greater than its value.
    @return `true` if the optional is disengaged or `lhs` is greater than its value; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalGtT<T, U>
operator>(T const& lhs, Optional<U> const& rhs)
{
    return !rhs || lhs > *rhs;
}

/** Checks if the Optional is less than or equal to a value.
    Returns true if the Optional is disengaged or its value is less than or equal to rhs.
    @return `true` if the optional is disengaged or less than or equal to `rhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalLeT<T, U>
operator<=(Optional<T> const& lhs, U const& rhs)
{
    return !lhs || *lhs <= rhs;
}

/** Checks if a value is less than or equal to an engaged Optional.
    Returns true if the Optional is engaged and lhs is less than or equal to its value.
    @return `true` if the optional is engaged and `lhs` is less than or equal to its value; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalLeT<T, U>
operator<=(T const& lhs, Optional<U> const& rhs)
{
    return rhs && lhs <= *rhs;
}

/** Checks if the Optional is greater than or equal to a value.
    Returns true if the Optional is engaged and its value is greater than or equal to rhs.
    @return `true` if the optional is engaged and greater than or equal to `rhs`; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<U>)
constexpr detail::OptionalGeT<T, U>
operator>=(Optional<T> const& lhs, U const& rhs)
{
    return lhs && *lhs >= rhs;
}

/** Checks if a value is greater than or equal to an Optional.
    Returns true if the Optional is disengaged or lhs is greater than or equal to its value.
    @return `true` if the optional is disengaged or `lhs` is greater than or equal to its value; otherwise, `false`.
*/
template <typename T, typename U>
requires(!detail::isOptionalV<T>)
constexpr detail::OptionalGeT<T, U>
operator>=(T const& lhs, Optional<U> const& rhs)
{
    return !rhs || lhs >= *rhs;
}

/** Performs a three-way comparison between an Optional and a value.
    If the Optional is engaged, compares its value to v; otherwise, returns less.
    @return The result of the three-way comparison with the value.
*/
template <typename T, typename U>
requires(!detail::isDerivedFromOptional<U>)
        && requires { typename std::compare_three_way_result_t<T, U>; }
        && std::three_way_comparable_with<T, U>
constexpr std::compare_three_way_result_t<T, U>
operator<=>(Optional<T> const& x, U const& v)
{
    return bool(x) ? *x <=> v : std::strong_ordering::less;
}

} // namespace clang::mrdocs

#endif
