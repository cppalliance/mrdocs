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

#include <algorithm>

namespace clang {
namespace mrdox {

/** Visit all elements of a range concurrently.
*/
template<
    class Elements,
    class Workers,
    class... Args>
Error
parallelFor(
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
