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
#include <iterator>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

//------------------------------------------------

/** A pool of threads for executing work concurrently.
*/
class MRDOX_VISIBLE
    ThreadPool
{
    class Impl;

    struct MRDOX_VISIBLE
        Work
    {
        virtual ~Work() = 0;
        virtual void operator()() = 0;
    };

    std::unique_ptr<Impl> impl_;

public:
    /** Destructor.
    */
    MRDOX_DECL
    ~ThreadPool();

    /** Constructor.
    */
    MRDOX_DECL
    explicit
    ThreadPool(
        std::size_t concurrency);

    /** Submit work to be executed.

        The signature of the submitted function
        object should be `void(void)`.
    */
    template<class F>
    void
    post(F&& f);

    /** Block until all work has completed.
    */
    MRDOX_DECL
    void
    wait();

private:
    MRDOX_DECL void do_post(std::unique_ptr<Work>);
};

template<class F>
void
ThreadPool::
post(F&& f)
{
    if(! impl_)
    {
        // no threads
        f();
        return;
    }

    struct WorkImpl : Work
    {
        F f;

        ~WorkImpl() = default;

        explicit WorkImpl(F&& f_)
            : f(std::forward<F>(f_))
        {
        }

        void operator()() override
        {
            f();
        }
    };

    do_post(std::make_unique<WorkImpl>(std::forward<F>(f)));
}

//------------------------------------------------

/** Visit all elements of a range concurrently.
*/
template<
    class Elements,
    class Workers,
    class... Args>
Error
forEach(
    Elements& elements,
    Workers& workers,
    Args&&... args)
{
    if(std::next(std::begin(workers)) == std::end(workers))
    {
        // Non-concurrent
        auto&& worker(*std::begin(workers));
        for(auto&& element : elements)
            if(! worker(element, std::forward<Args>(args)...))
                return Error("canceled");
        return Error::success();
    }

    std::mutex m;
    bool cancel = false;
    auto it = std::begin(elements);
    auto const end = std::end(elements);
    auto const do_work =
        [&](auto&& agent)
        {
            std::unique_lock<std::mutex> lock(m);
            if(it == end || cancel)
                return false;
            auto it0 = it;
            ++it;
            lock.unlock();
            bool cancel_ = ! agent(*it0,
                std::forward<Args>(args)...);
            if(! cancel_)
                return true;
            cancel = true;
            return false;
        };
    std::vector<std::thread> threads;
    for(auto& worker : workers)
        threads.emplace_back(std::thread(
            [&](auto&& agent)
            {
                for(;;)
                    if(! do_work(agent))
                        break;
            }, worker));
    for(auto& t : threads)
        t.join();
    if(cancel)
        return Error("canceled");
    return Error::success();
}

} // mrdox
} // clang

#endif
