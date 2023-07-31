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
#ifndef MRDOX_PLUGIN_HPP
#define MRDOX_PLUGIN_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>

namespace clang {
namespace mrdox {

class Generator;
class PluginEnvironment
{
public:
    virtual
    void
    addGenerator(std::unique_ptr<Generator> generator) = 0;
};

}
}

// the version gets delivers as an argument, because env might incompatible.
// this function must return false on version conflicts!
extern "C"
MRDOX_SYMBOL_EXPORT
bool
MrDoxMain(
    int versionMajor,
    int versionMinor,
    clang::mrdox::PluginEnvironment & env);

#endif //MRDOX_PLUGIN_HPP
