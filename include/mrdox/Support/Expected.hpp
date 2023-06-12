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
#include <new>
#include <type_traits>

namespace clang {
namespace mrdox {

/** A container holding an error or a value.
*/
template<class T>
class [[nodiscard]] Expected
{
    static_assert(! std::is_reference_v<T>);

    union
    {
        T t_;
        Error e_;
    };
    bool has_error_;

public:
    using value_type = T;
    using pointer = std::remove_reference_t<T>*;
    using reference = std::remove_reference_t<T>&;
    using const_pointer = std::remove_reference_t<T> const*;
    using const_reference = std::remove_reference_t<T> const&;

    Expected(Expected const&) = delete;
    Expected& operator=(Expected const&) = delete;

    /** Destructor.
    */
    ~Expected()
    {
        if(has_error_)
            e_.~Error();
        else
            t_.~T();
    }

    /** Constructor.

        @param err The error.
    */
    Expected(
        Error err) noexcept
        : has_error_(true)
    {
        MRDOX_ASSERT(err.failed());
        new(&e_) Error(std::move(err));
    }

    Expected(Expected&& other)
        : has_error_(other.has_error_)
    {
        if(other.has_error_)
            new(&e_) Error(std::move(other.e_));
        else
            new(&t_) T(std::move(other.t_));
    }

    template<class U>
    requires std::is_convertible_v<U, T>
    Expected(U&& u)
        : has_error_(false)
    {
        new(&t_) T(std::forward<U>(u));
    }

    Expected& operator=(Expected&& other)
    {
        if(this != &other)
        {
            this->~Expected();
            new(this) Expected(std::move(other));
        }
        return *this;
    }

    template<class U>
    requires std::is_convertible_v<U, T>
    Expected& operator=(Expected<U>&& other)
    {
        this->~Expected();
        new(this) Expected(std::move(other));
        return *this;
    }

    Error const&
    getError() const
    {
        MRDOX_ASSERT(has_error_);
        return e_;
    }

    bool has_value() const noexcept
    {
        return ! has_error_;
    }

    explicit operator bool()
    {
        return ! has_error_;
    }

    reference get() noexcept
    {
        MRDOX_ASSERT(! has_error_);
        return t_;
    };

    const_reference get() const noexcept
    {
        MRDOX_ASSERT(! has_error_);
        return t_;
    };

    pointer operator->() noexcept
    {
        MRDOX_ASSERT(! has_error_);
        return &t_;
    }

    reference operator*() noexcept
    {
        MRDOX_ASSERT(! has_error_);
        return t_;
    }

    const_pointer operator->() const noexcept
    {
        MRDOX_ASSERT(! has_error_);
        return &t_;
    }

    const_reference operator*() const noexcept
    {
        MRDOX_ASSERT(! has_error_);
        return t_;
    }
};

} // mrdox
} // clang

#endif
