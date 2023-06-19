//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ToolWorld.hpp"
#include "GeneratorsImpl.hpp"

#include <mrdox/Platform.hpp>
#include <mrdox/Generators.hpp>

namespace clang {
namespace mrdox {

static
ToolWorld *
        s_toolWorld = nullptr;

ToolWorld::ToolWorld() : generators(std::make_unique<GeneratorsImpl>())
{
    MRDOX_ASSERT(s_toolWorld == nullptr);
    s_toolWorld = this;
}

ToolWorld::~ToolWorld()
{
    MRDOX_ASSERT(s_toolWorld == this);
    s_toolWorld = nullptr;
}


ToolWorld& toolWorld() noexcept
{
    MRDOX_ASSERT(s_toolWorld != nullptr);
    return *s_toolWorld;
}

}
}
