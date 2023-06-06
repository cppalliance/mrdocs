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

#ifndef MRDOX_SUPPORT_PARALLELFOR_HPP
#define MRDOX_SUPPORT_PARALLELFOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <iterator>
#include <mutex>
#include <thread>
#include <vector>

namespace clang {
namespace mrdox {

/** Visit all elements of a range concurrently.
*/
template<
    class Range,
    class Worker,
    class... Args>
Error
parallelFor(
    Range&& range,
    std::vector<Worker>& workers,
    Args&&... args)
{
    std::mutex m;
    bool cancel = false;
    auto it = std::begin(range);
    auto const end = std::end(range);
    auto const do_work =
        [&](Worker& worker)
        {
            std::unique_lock<std::mutex> lock(m);
            if(it == end || cancel)
                return false;
            auto it0 = it;
            ++it;
            lock.unlock();
            bool cancel_ = worker(*it0,
                std::forward<Args>(args)...);
            if(! cancel_)
                return true;
            cancel = true;
            return false;
        };
    std::vector<std::thread> threads;
    threads.reserve(workers.size());
    for(std::size_t i = 0; i < workers.size(); ++i)
        threads.emplace_back(std::thread(
            [&](std::size_t i_)
            {
                for(;;)
                    if(! do_work(workers[i]))
                        break;
            }, i));
    for(auto& t : threads)
        t.join();
    if(cancel)
        return Error("canceled");
    return Error::success();
}

} // mrdox
} // clang

#endif
