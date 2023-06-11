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
#include <functional>
#include <iterator>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

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
    void
    async(std::function<void(void)> f);

    /** Invoke a function object for each element of a range.
    */
    template<class Range, class F>
    void forEach(Range&& range, F const& f);

    /** Block until all work has completed.
    */
    MRDOX_DECL
    void
    wait();
};

//------------------------------------------------

/** A subset of possible work in a thread pool.
*/
class MRDOX_VISIBLE
    TaskGroup
{
    std::unique_ptr<llvm::ThreadPoolTaskGroup> impl_;

public:
    MRDOX_DECL
    ~TaskGroup();

    MRDOX_DECL
    explicit
    TaskGroup(
        ThreadPool& threadPool);

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(void)`.
    */
    void
    async(std::function<void(void)> f);

    /** Block until all work has completed.
    */
    MRDOX_DECL
    void
    wait();
};

//------------------------------------------------

/** Invoke a function object for each element of a range.
*/
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

//------------------------------------------------

#if 0
/** A set of execution agents for performing concurrent work.
*/
template<class... Args>
class ExecutionGroup
{
    std::vector<Agent*> agents_;
    std::vector<std::function<void(Agent&)>> work_;
    ThreadPool threadPool_;
    TaskGroup taskGroup_;

public:
    template<class Agents>
    explicit
    Executors(
        Agents&& agents,
        ThreadPool &threadPool)
        : threadPool_(threadPool)
    {
        agents_.reserve(std::distance(
            std::begin(agents), std::end(agents)));
        for(auto& agent : agents)
            agents_.push_back(&agent);
    }

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(Agent&)`.
    */
    void
    async(std::function<void(void)> f);

    /** Block until all work has completed.
    */
    void
    wait()
    {
        taskGroup_.wait();
    }
};
#endif

} // mrdox
} // clang

#endif
