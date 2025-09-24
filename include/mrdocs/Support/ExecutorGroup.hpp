//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_EXECUTORGROUP_HPP
#define MRDOCS_API_SUPPORT_EXECUTORGROUP_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <mrdocs/Support/any_callable.hpp>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace clang {
namespace mrdocs {

class MRDOCS_DECL
    ExecutorGroupBase
{
    class scoped_agent;

protected:
    struct Impl;

    struct MRDOCS_DECL
        AnyAgent
    {
        virtual ~AnyAgent() = 0;
        virtual void* get() noexcept = 0;
    };

    std::unique_ptr<Impl> impl_;
    std::vector<std::unique_ptr<AnyAgent>> agents_;
    std::deque<any_callable<void(void*)>> work_;

    explicit ExecutorGroupBase(ThreadPool&);
    void post(any_callable<void(void*)>);
    void run(std::unique_lock<std::mutex>);

public:
    template<class T>
    using arg_t = ThreadPool::arg_t<T>;

    ~ExecutorGroupBase();
    ExecutorGroupBase(ExecutorGroupBase&&) noexcept;

    /** Block until all work has completed.

        @return Zero or more errors which were
        thrown from submitted work.
    */
    [[nodiscard]]
    std::vector<Error>
    wait() noexcept;
};

//------------------------------------------------

/** A set of execution agents for performing concurrent work.
*/
template<class Agent>
class ExecutorGroup : public ExecutorGroupBase
{
    struct AgentImpl : AnyAgent
    {
        Agent agent_;

        template<class... Args>
        AgentImpl(Args&&... args)
            : agent_(std::forward<Args>(args)...)
        {
        }

        void* get() noexcept override
        {
            return &agent_;
        }
    };

public:
    explicit
    ExecutorGroup(
        ThreadPool& threadPool)
        : ExecutorGroupBase(threadPool)
    {
    }

    /** Construct a new agent in the group.

        The behavior is undefined if there is
        any outstanding work or busy threads.

        @param args Zero or more arguments
        to forward to the agent constructor.
    */
    template<class... Args>
    void
    emplace(Args&&... args)
    {
        agents_.emplace_back(
            std::make_unique<AgentImpl>(
                std::forward<Args>(args)...));
    }

    /** Submit work to be executed.

        The function object must have this
        equivalent signature:
        @code
        void( Agent&, Args... );
        @endcode

        @param f The function to invoke.
        @param args Zero or more arguments to
        forward to the function.
    */
    template<class F, class... Args>
    void
    async(F&& f, Args&&... args)
    {
        static_assert(std::is_invocable_v<F, Agent&, arg_t<Args>...>);
        post(
            [
                f = std::forward<F>(f),
                args = std::tuple<arg_t<Args>...>(args...)
            ](void* agent)
            {
                std::apply(f,
                    std::tuple_cat(std::tuple<Agent&>(
                        *reinterpret_cast<Agent*>(agent)),
                    std::move(args)));
            });
    }
};

} // mrdocs
} // clang

#endif
