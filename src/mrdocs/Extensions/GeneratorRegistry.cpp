//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "../Generators/BuiltinGenerators.hpp"
#include <mrdocs/Extensions/GeneratorRegistry.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

GeneratorRegistry::GeneratorRegistry()
{
    Expected<void> const builtins = registerBuiltinGenerators(*this);
    MRDOCS_ASSERT(builtins);
}

GeneratorRegistry::~GeneratorRegistry() = default;

Expected<void>
GeneratorRegistry::install(std::shared_ptr<Generator const> generator)
{
    MRDOCS_CHECK(generator, "cannot install null generator");
    std::lock_guard<std::mutex> const lock(mutex_);
    for (std::shared_ptr<Generator const> const& g : generators_)
    {
        if (g == generator)
        {
            return {};
        }
        MRDOCS_CHECK(g->id() != generator->id(), formatError("generator id=\"{}\" already exists", g->id()));
    }
    generators_.push_back(std::move(generator));
    return {};
}

std::vector<Generator const*>
GeneratorRegistry::generators() const
{
    std::lock_guard<std::mutex> const lock(mutex_);
    std::vector<Generator const*> result;
    result.reserve(generators_.size());
    for (std::shared_ptr<Generator const> const& g : generators_)
    {
        result.push_back(g.get());
    }
    return result;
}

Generator const*
GeneratorRegistry::find(std::string_view id) const noexcept
{
    std::lock_guard<std::mutex> const lock(mutex_);
    for (std::shared_ptr<Generator const> const& g : generators_)
    {
        if (g->id() == id)
        {
            return g.get();
        }
    }
    return nullptr;
}

} // mrdocs
