//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_EXECUTORGROUP_HPP
#define MRDOX_SUPPORT_EXECUTORGROUP_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <mrdox/Support/unlock_guard.hpp>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

namespace clang {
namespace mrdox {

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
            MRDOX_ASSERT(agent_.get() != nullptr);
        }

        ~scoped_agent()
        {
            --group_.busy_;
            group_.agents_.emplace_back(std::move(agent_));
            group_.impl_->cv_.notify_all();
        }

        Agent& operator*() const noexcept
        {
            MRDOX_ASSERT(agent_.get() != nullptr);
            return *agent_;
        }
    };

    void
    run(std::unique_lock<std::mutex> lock)
    {
        MRDOX_ASSERT(lock.owns_lock());
        std::unique_ptr<Agent> agent(std::move(agents_.back()));
        MRDOX_ASSERT(agent.get() != nullptr);
        agents_.pop_back();
        ++busy_;

        threadPool_.async(
            [this, agent = std::move(agent)]() mutable
            {
                MRDOX_ASSERT(agent.get() != nullptr);
                scoped_agent scope(*this, std::move(agent));
                MRDOX_ASSERT(agent.get() == nullptr);
                std::unique_lock<std::mutex> lock(impl_->mutex_);
                for(;;)
                {
                    if(work_.empty())
                        break;
                    any_callable<void(Agent&)> work(std::move(work_.front()));
                    work_.pop_front();
                    unlock_guard unlock(impl_->mutex_);
                    work(*scope);
                }
            });
    }
};

} // mrdox
} // clang

#endif
