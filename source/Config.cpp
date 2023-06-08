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

#include "ConfigImpl.hpp"
#include "Support/Path.hpp"
#include <mrdox/Support/Error.hpp>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

#include <version>

// Check llvm version
#define STRINGIFY(Value) #Value
static_assert(
    LLVM_VERSION_MAJOR >= MRDOX_MINIMUM_LLVM_VERSION,
    "MrDox requires at least clang " STRINGIFY(MRDOX_MINIMUM_LLVM_VERSION)
    ", got " LLVM_VERSION_STRING " instead.");

#if ! defined(__cpp_lib_ranges)
#error "Ranges library unavailable"
#endif

#include <ranges>

#include <fmt/format.h>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Config::WorkGroup
//
//------------------------------------------------

class Config::WorkGroup::
    Impl : public Base
{
public:
    explicit
    Impl(
        llvm::ThreadPool& threadPool) noexcept
        : group_(threadPool)
    {
    }

    llvm::ThreadPoolTaskGroup group_;
};

Config::
WorkGroup::
Base::~Base() noexcept = default;

Config::
WorkGroup::
~WorkGroup()
{
    impl_ = nullptr;
    config_ = nullptr;
}

Config::
WorkGroup::
WorkGroup(
    Config const* config)
    : config_(config
        ? dynamic_cast<ConfigImpl const*>(
            config)->shared_from_this()
        : nullptr)
    , impl_(config_
        ? std::make_unique<Impl>(const_cast<
            llvm::ThreadPool&>(config_->threadPool_))
        : nullptr)
{
}

Config::
WorkGroup::
WorkGroup(
    WorkGroup const& other)
    : config_(other.config_)
    , impl_(other.config_
        ? std::make_unique<Impl>(const_cast<
            llvm::ThreadPool&>(config_->threadPool_))
        : nullptr)
{
}

auto
Config::
WorkGroup::
operator=(
    WorkGroup const& other) ->
        WorkGroup&
{
    if(this == &other)
        return *this;

    if(! other.config_)
    {
        config_ = nullptr;
        impl_ = nullptr;
        return *this;
    }

    auto impl = std::make_unique<Impl>(
        const_cast<llvm::ThreadPool&>(
            other.config_->threadPool_));
    impl_ = std::move(impl);
    config_ = other.config_;
    return *this;
}

void
Config::
WorkGroup::
post(
    std::function<void(void)> f)
{
    if(config_ && config_->concurrency > 1)
    {
        auto& impl = static_cast<Impl&>(*impl_);
        impl.group_.async(std::move(f));
    }
    else
    {
        f();
    }
}

void
Config::
WorkGroup::
wait()
{
    if(config_ && config_->concurrency > 1)
    {
        auto& impl = static_cast<Impl&>(*impl_);
        impl.group_.wait();
    }
}

//------------------------------------------------
//
// Config
//
//------------------------------------------------

Config::
Config() noexcept
{
}

Config::
~Config() noexcept = default;

} // mrdox
} // clang
