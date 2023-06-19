//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_TOOLWORLD_HPP
#define MRDOX_TOOL_TOOLWORLD_HPP

#include <memory>
#include <vector>

namespace clang {
namespace mrdox {


/** Holds the tool's global variables.
    A single object of this type is used to manage
    objects whose lifetime and order of destruction
    is sensitive.
*/
class ToolWorld
{
    ToolWorld();
    ToolWorld(const ToolWorld & ) = delete;
    ~ToolWorld();
    // Order of destruction here matters:
    // Members declared first are destroyed last.
    // remember: last gets unloaded first.
  public:
    std::unique_ptr<class Generators> generators;
    friend int mrdox_main(int argc, char const** argv);
};

/** Return the instance of the tool's global variables.
*/
ToolWorld & toolWorld() noexcept;

}
}

#endif //MRDOX_TOOL_TOOLWORLD_HPP
