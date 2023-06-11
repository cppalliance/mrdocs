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
#include <mrdox/Support/Thread.hpp>
#include <llvm/Support/ThreadPool.h>
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
async(
    std::function<void(void)> f)
{
    impl_->async(std::move(f));
}

void
ThreadPool::
wait()
{
    impl_->wait();
}

//------------------------------------------------
//
// TaskGroup
//
//------------------------------------------------

TaskGroup::
~TaskGroup()
{
}

TaskGroup::
TaskGroup(
    ThreadPool& threadPool)
    : impl_(std::make_unique<llvm::ThreadPoolTaskGroup>(*threadPool.impl_))
{
}

void
TaskGroup::
async(
    std::function<void(void)> f)
{
    impl_->async(std::move(f));
}

void
TaskGroup::
wait()
{
    impl_->wait();
}

} // mrdox
} // clang
