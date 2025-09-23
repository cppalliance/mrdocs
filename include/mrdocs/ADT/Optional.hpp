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
    requires std::is_constructible_v<T, U>
    constexpr explicit Optional(U&& u) noexcept(
        std::is_nothrow_constructible_v<T, U>)
        : s_([&] {
            if constexpr (uses_nullable_traits)
            {
                return storage_t(static_cast<T>(std::forward<U>(u)));
            }
            else
            {
                return storage_t(std::forward<U>(u));
            }
        }())
    {}

    /** Assign from a value.

        @param u The value to store. It must be convertible to T.
     **/
    template <class U>
    requires std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>
    constexpr Optional&
    operator=(U&& u) noexcept(std::is_nothrow_assignable_v<T&, U>)
    {
        s_ = std::forward<U>(u);
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

    /** Three-way comparison defaults when supported by the underlying
        T/std::optional.
     **/
    auto
    operator<=>(Optional const&) const
        = default;
};

} // namespace clang::mrdocs

#endif
