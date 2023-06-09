//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_PLUGIN_HPP
#define MRDOX_PLUGIN_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>

namespace clang {
namespace mrdox {

extern "C"
{

// implement this function if you want to provide your own generator
std::unique_ptr<Generator> makeMrDoxGenerator();

}

}
}

#endif //MRDOX_PLUGIN_HPP
