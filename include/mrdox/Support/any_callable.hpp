//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_ANY_CALLABLE_HPP
#define MRDOX_SUPPORT_ANY_CALLABLE_HPP

#include <mrdox/Platform.hpp>
#include <memory>
#include <type_traits>
#include <utility>

namespace clang {
namespace mrdox {

/** A movable, type-erased function object.

    Usage:
    @code
    any_callable<void(void)> f;
    @endcode
*/
template<class>
class any_callable;

template<class R, class... Args>
class any_callable<R(Args...)>
{
    struct base
    {
        virtual ~base() = default;
        virtual R invoke(Args&&...args) = 0;
    };

    std::unique_ptr<base> p_;

public:
    any_callable() = delete;

    template<class Callable>
    requires std::is_invocable_r_v<R, Callable, Args...>
    any_callable(Callable&& f)
    {
        class impl : public base
        {
            Callable f_;

        public:
            explicit impl(Callable&& f)
                : f_(std::forward<Callable>(f))
            {
            }

            R invoke(Args&&... args) override
            {
                return f_(std::forward<Args>(args)...);
            }
        };

        p_ = std::make_unique<impl>(std::forward<Callable>(f));
    }

    R operator()(Args&&...args) const
    {
        return p_->invoke(std::forward<Args>(args)...);
    }
};

} // mrdox
} // clang

#endif
