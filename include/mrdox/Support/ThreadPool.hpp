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

#ifndef MRDOX_SUPPORT_THREAD_HPP
#define MRDOX_SUPPORT_THREAD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/any_callable.hpp>
#include <memory>
#include <type_traits>
#include <utility>

namespace llvm {
class ThreadPool;
class ThreadPoolTaskGroup;
} // llvm

namespace clang {
namespace mrdox {

class TaskGroup;

//------------------------------------------------

/** A pool of threads for executing work concurrently.
*/
class MRDOX_VISIBLE
    ThreadPool
{
    std::unique_ptr<llvm::ThreadPool> impl_;

    friend class TaskGroup;

public:
    template<class Agent> struct arg_ty { using type = Agent; };
    template<class Agent> struct arg_ty<Agent&> { using type =
        std::conditional_t< std::is_const_v<Agent>, Agent, Agent&>; };
    template<class Agent> using arg_t = typename arg_ty<Agent>::type;

    /** Destructor.
    */
    MRDOX_DECL
    ~ThreadPool();

    /** Constructor.

        Default constructed thread pools may only
        be reset or destroyed.
    */
    MRDOX_DECL
    explicit
    ThreadPool();

    /** Constructor.
    */
    MRDOX_DECL
    explicit
    ThreadPool(
        unsigned concurrency);

    /** Reset the pool to the specified concurrency.
    */
    MRDOX_DECL
    void
    reset(
        unsigned concurrency);

    /** Return the number of threads in the pool.
    */
    MRDOX_DECL
    unsigned
    getThreadCount() const noexcept;

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(void)`.
    */
    template<class F>
    void
    async(F&& f)
    {
        post(std::forward<F>(f));
    }

    /** Invoke a function object for each element of a range.
    */
    template<class Range, class F>
    void forEach(Range&& range, F const& f);

    /** Block until all work has completed.
    */
    MRDOX_DECL
    void
    wait();

private:
    MRDOX_DECL void post(any_callable<void(void)>);
};

//------------------------------------------------

/** A subset of possible work in a thread pool.
*/
class MRDOX_VISIBLE
    TaskGroup
{
    std::unique_ptr<llvm::ThreadPoolTaskGroup> impl_;

public:
    /** Destructor.
    */
    MRDOX_DECL
    ~TaskGroup();

    /** Constructor.
    */
    MRDOX_DECL
    explicit
    TaskGroup(
        ThreadPool& threadPool);

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(void)`.
    */
    template<class F>
    void
    async(F&& f)
    {
        post(std::forward<F>(f));
    }

    /** Block until all work has completed.
    */
    MRDOX_DECL
    void
    wait();

private:
    MRDOX_DECL void post(any_callable<void(void)>);
};

//------------------------------------------------

template<class Range, class F>
void
ThreadPool::
forEach(
    Range&& range,
    F const& f)
{
    TaskGroup taskGroup(*this);
    for(auto&& value : range)
        taskGroup.async(
            [&f, &value]
            {
                f(value);
            });
    taskGroup.wait();
}

} // mrdox
} // clang

#endif
