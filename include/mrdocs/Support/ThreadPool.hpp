//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_THREAD_HPP
#define MRDOCS_API_SUPPORT_THREAD_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/any_callable.hpp>
#include <mrdocs/Support/Error.hpp>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace llvm {
class StdThreadPool;
class ThreadPoolTaskGroup;
} // llvm

namespace clang {
namespace mrdocs {

class TaskGroup;

//------------------------------------------------

/** A pool of threads for executing work concurrently.
*/
class MRDOCS_VISIBLE
    ThreadPool
{
    std::unique_ptr<llvm::StdThreadPool> impl_;

    friend class TaskGroup;

public:
    template<class T> struct arg_ty { using type = T; };
    template<class T> struct arg_ty<T&> { using type =
        std::conditional_t< std::is_const_v<T>, T, T&>; };
    template<class T> using arg_t = typename arg_ty<T>::type;

    /** Destructor.
    */
    MRDOCS_DECL
    ~ThreadPool();

    /** Constructor.

        Default constructed thread pools have
        concurrency equal to one and never spawn
        new threads. Submitted work blocks the
        caller until the work is complete.
    */
    MRDOCS_DECL
    explicit
    ThreadPool();

    /** Constructor.
    */
    MRDOCS_DECL
    explicit
    ThreadPool(
        unsigned concurrency);

    /** Return the number of threads in the pool.
    */
    MRDOCS_DECL
    unsigned
    getThreadCount() const noexcept;

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(void)`.

        @param f The function object to execute.
    */
    template<class F>
    void
    async(F&& f)
    {
        post(std::forward<F>(f));
    }

    /** Invoke a function object for each element of a range.

        @param range The range of elements to process.
        @param f The function object to invoke.

        @return Zero or more errors which were
        thrown from submitted work.

    */
    template<class Range, class F>
    [[nodiscard]]
    std::vector<Error>
    forEach(Range&& range, F const& f);

    /** Block until all work has completed.
    */
    MRDOCS_DECL
    void
    wait();

private:
    MRDOCS_DECL void post(any_callable<void(void)>);
};

//------------------------------------------------

/** A subset of possible work in a thread pool.
*/
class MRDOCS_VISIBLE
    TaskGroup
{
    struct Impl;

    std::unique_ptr<Impl> impl_;

public:
    /** Destructor.
    */
    MRDOCS_DECL
    ~TaskGroup();

    /** Constructor.
    */
    MRDOCS_DECL
    explicit
    TaskGroup(
        ThreadPool& threadPool);

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(void)`.

        @param f The function object to execute.
    */
    template<class F>
    void
    async(F&& f)
    {
        post(std::forward<F>(f));
    }

    /** Block until all work has completed.

        @return Zero or more errors which were
        thrown from submitted work.
    */
    MRDOCS_DECL
    [[nodiscard]]
    std::vector<Error>
    wait();

private:
    MRDOCS_DECL void post(any_callable<void(void)>);
};

//------------------------------------------------

template<class Range, class F>
std::vector<Error>
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
    return taskGroup.wait();
}

} // mrdocs
} // clang

#endif
