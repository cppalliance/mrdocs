//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Debug.hpp"
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <llvm/Support/Signals.h>
#include <llvm/Support/ThreadPool.h>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// ThreadPool
//
//------------------------------------------------

ThreadPool::
~ThreadPool()
{
}

ThreadPool::
ThreadPool() = default;

ThreadPool::
ThreadPool(
    unsigned concurrency)
{
    reset(concurrency);
}

void
ThreadPool::
reset(
    unsigned concurrency)
{
    llvm::ThreadPoolStrategy S;
    S.ThreadsRequested = concurrency;
    S.Limit = true;
    impl_ = std::make_unique<llvm::ThreadPool>(S);
}

unsigned
ThreadPool::
getThreadCount() const noexcept
{
    return impl_->getThreadCount();
}

void
ThreadPool::
wait()
{
    impl_->wait();
}

void
ThreadPool::
post(
    any_callable<void(void)> f)
{
    auto sp = std::make_shared<
        any_callable<void(void)>>(std::move(f));
    impl_->async(
    [sp]
    {
        try
        {
            (*sp)();
        }
        catch(std::exception const& ex)
        {
            // Any exception which is not
            // derived from Error should
            // be reported and terminate
            // the process immediately.
            reportUnhandledException(ex);
        }
    });
}

//------------------------------------------------
//
// TaskGroup
//
//------------------------------------------------

struct TaskGroup::
    Impl
{
    std::mutex mutex;
    std::unordered_set<Error> errors;
    llvm::ThreadPoolTaskGroup taskGroup;

    explicit
    Impl(
        llvm::ThreadPool& threadPool)
        : taskGroup(threadPool)
    {
    }
};


TaskGroup::
~TaskGroup()
{
}

TaskGroup::
TaskGroup(
    ThreadPool& threadPool)
    : impl_(std::make_unique<Impl>(*threadPool.impl_))
{
}

std::vector<Error>
TaskGroup::
wait()
{
    impl_->taskGroup.wait();

    // VFALCO We could have a small data race here
    // where another thread posts work after the
    // wait is satisfied, but that could be
    // considered user error.
    //
    // In theory the lock should not be needed.
    //
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<Error> errors;
    errors.reserve(impl_->errors.size());
    for(auto& err : impl_->errors)
        errors.emplace_back(std::move(err));
    impl_->errors.clear();
    return errors;
}

void
TaskGroup::
post(
    any_callable<void(void)> f)
{
    auto sp = std::make_shared<
        any_callable<void(void)>>(std::move(f));
    impl_->taskGroup.async(
    [&, sp]
    {
        try
        {
            (*sp)();
        }
        catch(Exception const& ex)
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->errors.emplace(ex.error());
        }
        catch(std::exception const& ex)
        {
            // Any exception which is not
            // derived from Error should
            // be reported and terminate
            // the process immediately.
            reportUnhandledException(ex);
        }
    });
}

} // mrdox
} // clang
