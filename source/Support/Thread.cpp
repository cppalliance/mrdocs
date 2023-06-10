//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Thread.hpp>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace clang {
namespace mrdox {

ThreadPool::Work::~Work() = default;

class ThreadPool::Impl
{
    std::vector<std::thread> threads_;
    std::condition_variable work_cv_;
    std::condition_variable join_cv_;
    std::mutex mutex_;

    std::deque<std::unique_ptr<Work>> work_;
    std::size_t busy_ = 0;
    bool destroy_ = false;

    void
    run()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++busy_;
        for(;;)
        {
            --busy_;
            work_cv_.wait(lock,
                [&]
                {
                    return destroy_ || ! work_.empty();
                });
            ++busy_;
            if(! work_.empty())
            {
                std::unique_ptr<Work> work =
                    std::move(work_.front());
                work_.pop_front();
                lock.unlock();
                (*work)();
                continue;
            }
            if(destroy_)
                break;
        }
        --busy_;
        join_cv_.notify_all();
    }

public:
    explicit
    Impl(std::size_t concurrency)
    {
        threads_.resize(concurrency);
        for(auto& t : threads_)
            t = std::thread(&Impl::run, this);
    }

    ~Impl()
    {
        for(auto& t : threads_)
            t.join();
    }

    void destroy() noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        destroy_ = true;
        work_cv_.notify_all();
    }

    void post(std::unique_ptr<Work> work)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        work_.emplace_back(std::move(work));
        work_cv_.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        join_cv_.wait(lock,
            [&]
            {
                return busy_ == 0;
            });
    }
};

ThreadPool::
~ThreadPool()
{
    if(impl_)
    {
        wait();
        impl_->destroy();
    }
}

ThreadPool::
ThreadPool(
    std::size_t concurrency)
{
    if( concurrency == 0)
        concurrency = std::thread::hardware_concurrency();
    if(concurrency > 1)
        impl_ = std::make_unique<Impl>(concurrency);
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
do_post(
    std::unique_ptr<Work> work)
{
    impl_->post(std::move(work));
}

} // mrdox
} // clang
