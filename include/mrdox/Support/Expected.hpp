//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_EXPECTED_HPP
#define MRDOX_API_SUPPORT_EXPECTED_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <memory>
#include <type_traits>
#include <utility>

namespace clang {
namespace mrdox {

/** A container holding an error or a value.
*/
template<class T>
class [[nodiscard]]
    Expected
{
    static_assert(! std::is_reference_v<T>);
    static_assert(! std::convertible_to<T, Error>);
    static_assert(std::move_constructible<T>);
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_move_constructible_v<Error>);

    union
    {
        T v_;
        Error e_;
    };
    bool has_error_;

public:
    using value_type = T;
    using error_type = Error;

    Expected(Expected const&) = delete;
    Expected& operator=(Expected const&) = delete;

    ~Expected();
    Expected(Error err) noexcept;
    Expected(Expected&& other) noexcept;
    Expected& operator=(Expected&& other) noexcept;

    template<class U>
    requires std::convertible_to<U, T>
    Expected(U&& u);

    template<class U>
    requires std::convertible_to<U, T>
    Expected& operator=(Expected<U>&& other) noexcept;

    // observers

    constexpr bool has_value() const noexcept;
    constexpr bool has_error() const noexcept;
    constexpr explicit operator bool() const noexcept;

    // checked value access

    constexpr T& value() &;
    constexpr T value() &&;
    constexpr T const& value() const&;
    constexpr T value() const&& = delete;
    constexpr T release() noexcept;

    // unchecked access

    constexpr T* operator->() noexcept;
    constexpr T& operator*() & noexcept;
    constexpr T operator*() &&;

    constexpr T const* operator->() const noexcept;
    constexpr T const& operator*() const& noexcept;
    constexpr T operator*() const&& noexcept = delete;

    // error access

    Error error() const& noexcept;
    Error error() && noexcept;

    // swap

    void swap(Expected& rhs) noexcept;

    friend void swap(
        Expected& lhs, Expected& rhs) noexcept
    {
        lhs.swap(rhs);
    }
};

template<class T>
Expected<T>::
~Expected()
{
    if(has_error_)
        std::destroy_at(&e_);
    else
        std::destroy_at(&v_);
}

template<class T>
Expected<T>::
Expected(
    Error err) noexcept
    : has_error_(true)
{
    MRDOX_ASSERT(err.failed());
    std::construct_at(&e_, std::move(err));
}

template<class T>
Expected<T>::
Expected(
    Expected&& other) noexcept
    : has_error_(other.has_error_)
{
    if(other.has_error_)
        std::construct_at(&e_, std::move(other.e_));
    else
        std::construct_at(&v_, std::move(other.v_));
}

template<class T>
auto
Expected<T>::
operator=(
    Expected&& other) noexcept ->
        Expected&
{
    if(this != &other)
    {
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
    }
    return *this;
}

template<class T>
template<class U>
requires std::convertible_to<U, T>
Expected<T>::
Expected(
    U&& u)
    : has_error_(false)
{
    static_assert(std::is_nothrow_convertible_v<U, T>);
    static_assert(! std::convertible_to<U, Error>);

    std::construct_at(&v_, std::forward<U>(u));
}

template<class T>
template<class U>
requires std::convertible_to<U, T>
auto
Expected<T>::
operator=(
    Expected<U>&& other) noexcept ->
        Expected&
{
    static_assert(std::is_nothrow_convertible_v<U, T>);
    static_assert(! std::convertible_to<U, Error>);

    Expected temp(std::move(other));
    temp.swap(*this);
    return *this;
}

template<class T>
constexpr
bool
Expected<T>::
has_value() const noexcept
{
    return ! has_error_;
}

template<class T>
constexpr
bool
Expected<T>::
has_error() const noexcept
{
    return has_error_;
}

template<class T>
constexpr
Expected<T>::
operator bool() const noexcept
{
    return ! has_error_;
}

template<class T>
constexpr
T&
Expected<T>::
value() &
{
    if(has_value())
        return v_;
    throw e_;
}

template<class T>
constexpr
T const&
Expected<T>::
value() const&
{
    if(has_value())
        return v_;
    throw e_;
}

template<class T>
constexpr
T
Expected<T>::
value() &&
{
    return std::move(value());
}

template<class T>
constexpr
T
Expected<T>::
release() noexcept
{
    return std::move(value());
}

template<class T>
constexpr
T*
Expected<T>::
operator->() noexcept
{
    return &v_;
}

template<class T>
constexpr
T&
Expected<T>::
operator*() & noexcept
{
    auto const p = operator->();
    MRDOX_ASSERT(p != nullptr);
    return *p;
}

template<class T>
constexpr
T
Expected<T>::
operator*() &&
{
    return std::move(**this);
}

template<class T>
Error
Expected<T>::
error() && noexcept
{
    if(has_error())
        return std::move(e_);
    return Error::success();
}

template<class T>
constexpr
T const*
Expected<T>::
operator->() const noexcept
{
    return &v_;
}

template<class T>
constexpr
T const&
Expected<T>::
operator*() const& noexcept
{
    auto const p = operator->();
    MRDOX_ASSERT(p != nullptr);
    return *p;
}

template<class T>
Error
Expected<T>::
error() const& noexcept
{
    if(has_error())
        return e_;
    return Error::success();
}

template<class T>
void
Expected<T>::
swap(Expected& rhs) noexcept
{
    using std::swap;
    if(has_error_)
    {
        if(rhs.has_error)
        {
            swap(e_, rhs.e_);
            return;
        }
        Error err(std::move(e_));
        std::destroy_at(&e_);
        std::construct_at(&v_, std::move(rhs.v_));
        std::destroy_at(&rhs.v_);
        std::construct_at(&rhs.e_, std::move(err));
        swap(has_error_, rhs.has_error_);
        return;
    }
    if(! rhs.has_error)
    {
        swap(v_, rhs.v_);
        return;
    }
    Error err(std::move(rhs.e_));
    std::destroy_at(&rhs.e_);
    std::construct_at(&rhs.v_, std::move(v_));
    std::destroy_at(&v_);
    std::construct_at(&e_, std::move(err));
    swap(has_error_, rhs.has_error_);
}

} // mrdox
} // clang

#endif
