//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Debug.hpp"
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <llvm/Support/Signals.h>
#include <llvm/Support/ThreadPool.h>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace clang {
namespace mrdocs {

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
    if(concurrency != 1)
    {
        llvm::ThreadPoolStrategy S;
        S.ThreadsRequested = concurrency;
        S.Limit = true;
        impl_ = std::make_unique<llvm::ThreadPool>(S);
    }
}

unsigned
ThreadPool::
getThreadCount() const noexcept
{
    if(impl_)
        return impl_->getThreadCount();
    return 1;
}

void
ThreadPool::
wait()
{
    if( impl_)
        impl_->wait();
}

void
ThreadPool::
post(
    any_callable<void(void)> f)
{
    if(impl_)
    {
        impl_->async(
        [sp = std::make_shared<
            any_callable<void(void)>>(std::move(f))]
        {
            // do NOT catch exceptions here
            (*sp)();
        });
        return;
    }

    // do NOT catch exceptions here
    f();
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
    std::unique_ptr<
        llvm::ThreadPoolTaskGroup> taskGroup;

    explicit
    Impl(
        llvm::ThreadPool* threadPool)
        : taskGroup(threadPool
            ? std::make_unique<
                llvm::ThreadPoolTaskGroup>(*threadPool)
            : nullptr)
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
    : impl_(std::make_unique<Impl>(
        threadPool.impl_.get()))
{
}

std::vector<Error>
TaskGroup::
wait()
{
    if( impl_->taskGroup)
        impl_->taskGroup->wait();

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
    if(impl_->taskGroup)
    {
        impl_->taskGroup->async(
        [&, sp = std::make_shared<
            any_callable<void(void)>>(std::move(f))]
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
                std::lock_guard<std::mutex> lock(impl_->mutex);
                impl_->errors.emplace(Error(ex));
            }
        });
        return;
    }

    try
    {
        f();
    }
    catch(Exception const& ex)
    {
        impl_->errors.emplace(ex.error());
    }
    catch(std::exception const& ex)
    {
        impl_->errors.emplace(Error(ex));
    }
}

} // mrdocs
} // clang
