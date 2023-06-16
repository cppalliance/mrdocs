//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/ExecutorGroup.hpp>
#include <mrdox/Support/Report.hpp>
#include <mrdox/Support/unlock_guard.hpp>
#include <condition_variable>
#include <unordered_set>

namespace clang {
namespace mrdox {

struct ExecutorGroupBase::
    Impl
{
    ThreadPool& threadPool;
    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_set<Error> errors;
    std::size_t busy = 0;

    explicit
    Impl(ThreadPool& threadPool_)
        : threadPool(threadPool_)
    {
    }
};

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
        --group_.impl_->busy;
        group_.agents_.emplace_back(std::move(agent_));
        group_.impl_->cv.notify_all();
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
    : impl_(std::make_unique<Impl>(threadPool))
{
}

ExecutorGroupBase::
~ExecutorGroupBase() = default;

ExecutorGroupBase::
ExecutorGroupBase(
    ExecutorGroupBase&&) noexcept = default;

void
ExecutorGroupBase::
post(any_callable<void(void*)> work)
{
    std::unique_lock<std::mutex> lock(impl_->mutex);
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
    ++impl_->busy;

    impl_->threadPool.async(
        [this, agent = std::move(agent)]() mutable
        {
            std::unique_lock<std::mutex> lock(impl_->mutex);
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
                    catch(Error const& err)
                    {
                        lock.lock();
                        impl_->errors.emplace(err);
                    }
                    catch(std::exception const& ex)
                    {
                        // Any exception which is not
                        // derived from Error should
                        // be reported and terminate
                        // the process immediately.
                        reportUnhandledException(ex);
                    }
                }
            }
        });
}

std::vector<Error>
ExecutorGroupBase::
wait() noexcept
{
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->cv.wait(lock,
        [&]
        {
            return work_.empty() && impl_->busy == 0;
        });
    std::vector<Error> errors;
    errors.reserve(impl_->errors.size());
    for(auto& err : impl_->errors)
        errors.emplace_back(std::move(err));
    impl_->errors.clear();
    return errors;
}

} // mrdox
} // clang
