//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ANY_CALLABLE_HPP
#define MRDOCS_API_SUPPORT_ANY_CALLABLE_HPP

#include <mrdocs/Platform.hpp>
#include <memory>
#include <type_traits>
#include <utility>


namespace mrdocs {

/** A movable, type-erased function object.

    Usage:
    @code
    any_callable<void(void)> f;
    @endcode
*/
template<class>
class any_callable;

/** Type-erased callable wrapper for signature `R(Args...)`.
*/
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
    /** Deleted default constructor to prevent empty call targets.
    */
    any_callable() = delete;

    /** Construct from a callable object matching the signature.
        @param f Callable to store; must satisfy `R(Args...)`.
    */
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

    /** Invoke the stored callable.
        @param args Arguments forwarded to the callable.
        @return Result of the wrapped callable.
    */
    R operator()(Args&&...args) const
    {
        return p_->invoke(std::forward<Args>(args)...);
    }
};

} // mrdocs


#endif
