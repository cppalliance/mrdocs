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
#include <atomic>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>
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

class unlock_guard
{
    std::mutex& m_;

public:
    explicit
    unlock_guard(std::mutex& m)
        : m_(m)
    {
        m_.unlock();
    }

    ~unlock_guard()
    {
        m_.lock();
    }
};

//------------------------------------------------

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

/** A set of execution agents for performing concurrent work.
*/
template<class Agent>
class ExecutorGroup
{
    struct Impl
    {
        std::mutex mutex_;
        std::condition_variable cv_;
    };

    ThreadPool& threadPool_;
    std::unique_ptr<Impl> impl_;
    std::vector<std::unique_ptr<Agent>> agents_;
    std::deque<any_callable<void(Agent&)>> work_;
    std::size_t busy_ = 0;

public:
    template<class T>
    using arg_t = ThreadPool::arg_t<T>;

    ExecutorGroup(ExecutorGroup const&) = delete;
    ExecutorGroup& operator=(ExecutorGroup&&) = delete;
    ExecutorGroup& operator=(ExecutorGroup const&) = delete;
    ExecutorGroup(ExecutorGroup&&) = default;

    explicit
    ExecutorGroup(
        ThreadPool& threadPool) noexcept
        : threadPool_(threadPool)
        , impl_(std::make_unique<Impl>())
    {
    }

    /** Construct a new agent in the group.

        The behavior is undefined if there is
        any outstanding work or busy threads.
    */
    template<class... Args>
    void
    emplace(Args&&... args)
    {
        agents_.emplace_back(std::make_unique<Agent>(
            std::forward<Args>(args)...));
    }

    /** Submit work to be executed.

        The function object must have this
        equivalent signature:
        @code
        void( Agent&, Args... );
        @endcode
    */
    template<class F, class... Args>
    void
    async(F&& f, Args&&... args)
    {
        static_assert(std::is_invocable_v<F, Agent&, arg_t<Args>...>);
        std::unique_lock<std::mutex> lock(impl_->mutex_);
        work_.emplace_back(
            [
                f = std::forward<F>(f),
                args = std::tuple<arg_t<Args>...>(args...)
            ](Agent& agent)
            {
                std::apply(f, std::tuple_cat(
                    std::tuple<Agent&>(agent),
                    std::move(args)));
            });
        if(agents_.empty())
            return;
        run(std::move(lock));
    }

    /** Block until all work has completed.
    */
    void
    wait()
    {
        std::unique_lock<std::mutex> lock(impl_->mutex_);
        impl_->cv_.wait(lock,
            [&]
            {
                return work_.empty() && busy_ == 0;
            });
    }

private:
    class scoped_agent
    {
        ExecutorGroup& group_;
        std::unique_ptr<Agent> agent_;

    public:
        scoped_agent(
            ExecutorGroup& group,
            std::unique_ptr<Agent> agent) noexcept
            : group_(group)
            , agent_(std::move(agent))
        {
        }

        ~scoped_agent()
        {
            --group_.busy_;
            group_.agents_.emplace_back(std::move(agent_));
            group_.impl_->cv_.notify_all();
        }
    };

    void
    run(std::unique_lock<std::mutex> lock)
    {
        std::unique_ptr<Agent> agent(std::move(agents_.back()));
        agents_.pop_back();
        ++busy_;

        threadPool_.async(
            [this, agent = std::move(agent)]() mutable
            {
                scoped_agent scope(*this, std::move(agent));
                std::unique_lock<std::mutex> lock(impl_->mutex_);
                for(;;)
                {
                    if(work_.empty())
                        break;
                    any_callable<void(Agent&)> work(std::move(work_.front()));
                    work_.pop_front();
                    unlock_guard unlock(impl_->mutex_);
                    work(*agent);
                }
            });
    }
};

} // mrdox
} // clang

#endif
