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

#include <mrdocs/Support/Concurrency/ExecutorGroup.hpp>
#include <mrdocs/Support/Concurrency/unlock_guard.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <condition_variable>
#include <unordered_set>


namespace mrdocs {

class ExecutorGroupBase::
    scoped_agent
{
    ExecutorGroupBase& group_;
    std::unique_ptr<AnyAgent> agent_;

public:
    scoped_agent(
        ExecutorGroupBase& group,
        std::unique_ptr<AnyAgent> agent) noexcept
        : group_(group)
        , agent_(std::move(agent))
    {
    }

    ~scoped_agent()
    {
        --group_.busy_;
        group_.agents_.emplace_back(std::move(agent_));
        group_.cv_.notify_all();
    }

    void* get() const noexcept
    {
        return agent_->get();
    }
};

ExecutorGroupBase::
AnyAgent::
~AnyAgent() = default;

ExecutorGroupBase::
ExecutorGroupBase(
    ThreadPool& threadPool)
    : threadPool_(threadPool)
{
}

ExecutorGroupBase::
~ExecutorGroupBase() = default;

ExecutorGroupBase::
ExecutorGroupBase(
    ExecutorGroupBase&& other) noexcept
    : threadPool_(other.threadPool_)
    , errors_(std::move(other.errors_))
    , busy_(other.busy_)
    , agents_(std::move(other.agents_))
    , work_(std::move(other.work_))
{
    // mutex_ and cv_ are freshly constructed rather than moved (neither is
    // movable). A group is only moved before any work has been posted, so
    // there is no in-flight state guarded by them to carry over.
}

void
ExecutorGroupBase::
post(any_callable<void(void*)> work)
{
    std::unique_lock<std::mutex> lock(mutex_);
    work_.emplace_back(std::move(work));
    if(agents_.empty())
        return;
    run(std::move(lock));
}

void
ExecutorGroupBase::
run(std::unique_lock<std::mutex> lock)
{
    std::unique_ptr<AnyAgent> agent(std::move(agents_.back()));
    agents_.pop_back();
    ++busy_;
    lock.unlock();

    threadPool_.async(
    [this, agent = std::move(agent)]() mutable
    {
        std::unique_lock<std::mutex> lock(mutex_);
        scoped_agent scope(*this, std::move(agent));
        for(;;)
        {
            if(work_.empty())
                break;
            any_callable<void(void*)> work(
                std::move(work_.front()));
            work_.pop_front();
            {
                lock.unlock();
                try
                {
                    work(scope.get());
                    lock.lock();
                }
                catch(Exception const& ex)
                {
                    lock.lock();
                    errors_.emplace(ex.error());
                }
                catch(std::exception const& ex)
                {
                    lock.lock();
                    errors_.emplace(Error(ex));
                }
            }
        }
    });
}

std::vector<Error>
ExecutorGroupBase::
wait() noexcept
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock,
        [&]
        {
            return work_.empty() && busy_ == 0;
        });
    std::vector<Error> errors;
    errors.reserve(errors_.size());
    for(auto& err : errors_)
        errors.emplace_back(std::move(err));
    errors_.clear();
    return errors;
}

} // mrdocs

