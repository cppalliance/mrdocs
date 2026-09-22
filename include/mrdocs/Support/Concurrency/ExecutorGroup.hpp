//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_CONCURRENCY_EXECUTORGROUP_HPP
#define MRDOCS_API_SUPPORT_CONCURRENCY_EXECUTORGROUP_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Concurrency/ThreadPool.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/TypeTraits/any_callable.hpp>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>


namespace mrdocs {

/** Base class that owns a pool of execution agents and a shared work queue.
*/
class MRDOCS_DECL ExecutorGroupBase
{
    class scoped_agent;

protected:
    /** Type-erased agent holder used by the base class.
    */
    struct AnyAgent
    {
        /** Virtual destructor to allow deleting through the base pointer.
        */
        virtual ~AnyAgent() = 0;
        /** Return a pointer to the stored agent object.
        */
        virtual void* get() noexcept = 0;
    };

    /** Pool that owns the worker threads.
    */
    ThreadPool& threadPool_;
    /** Guards the work queue, agent list, and error set.
    */
    std::mutex mutex_;
    /** Signals when a worker finishes so a waiter can re-check for idle.
    */
    std::condition_variable cv_;
    /** Errors thrown by submitted work.
    */
    std::unordered_set<Error> errors_;
    /** Number of worker threads currently running.
    */
    std::size_t busy_ = 0;
    /** Agents owned by the group.
    */
    std::vector<std::unique_ptr<AnyAgent>> agents_;
    /** Pending work posted to the group.
    */
    std::deque<any_callable<void(void*)>> work_;

    /** Construct with a backing thread pool.
    */
    explicit ExecutorGroupBase(ThreadPool&);
    /** Queue work to run on the group agents.
    */
    void post(any_callable<void(void*)> work);
    /** Execute queued work until empty.
        @param lock Held lock protecting the work queue.
    */
    void run(std::unique_lock<std::mutex> lock);

public:
    template<class T>
    /** Argument wrapper propagated from ThreadPool.
    */
    using arg_t = ThreadPool::arg_t<T>;

    /** Destroy the executor group, waiting for outstanding work.
    */
    ~ExecutorGroupBase();
    /** Move-construct from another group.
    */
    ExecutorGroupBase(ExecutorGroupBase&&) noexcept;

    /** Block until all work has completed.

        @return Zero or more errors which were
        thrown from submitted work.
    */
    [[nodiscard]]
    std::vector<Error>
    wait() noexcept;
};

//------------------------------------------------

/** A set of execution agents for performing concurrent work.
*/
template<class Agent>
class ExecutorGroup : public ExecutorGroupBase
{
    struct AgentImpl : AnyAgent
    {
        Agent agent_;

        template<class... Args>
        AgentImpl(Args&&... args)
            : agent_(std::forward<Args>(args)...)
        {
        }

        void* get() noexcept override
        {
            return &agent_;
        }
    };

public:
    /** Construct a new executor group bound to a thread pool.
        @param threadPool Pool that owns the worker threads.
    */
    explicit
    ExecutorGroup(
        ThreadPool& threadPool)
        : ExecutorGroupBase(threadPool)
    {
    }

    /** Construct a new agent in the group.

        The behavior is undefined if there is
        any outstanding work or busy threads.

        @param args Zero or more arguments
        to forward to the agent constructor.
    */
    template<class... Args>
    void
    emplace(Args&&... args)
    {
        agents_.emplace_back(
            std::make_unique<AgentImpl>(
                std::forward<Args>(args)...));
    }

    /** Submit work to be executed.

        The function object must have this
        equivalent signature:
        @code
        void( Agent&, Args... );
        @endcode

        @param f The function to invoke.
        @param args Zero or more arguments to
        forward to the function.
    */
    template<class F, class... Args>
    void
    async(F&& f, Args&&... args)
    {
        static_assert(std::is_invocable_v<F, Agent&, arg_t<Args>...>);
        post(
            [
                f = std::forward<F>(f),
                args = std::tuple<arg_t<Args>...>(args...)
            ](void* agent)
            {
                std::apply(f,
                    std::tuple_cat(std::tuple<Agent&>(
                        *reinterpret_cast<Agent*>(agent)),
                    std::move(args)));
            });
    }
};

} // mrdocs


#endif
