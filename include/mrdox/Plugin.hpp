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
class MRDOX_DECL PluginInfo
{
 public:
  const std::size_t size;
  const int abiVersion;

  bool
  requireVersion(
      int abiVersion_) const;
};

static_assert(std::is_standard_layout_v<PluginInfo>);

}
}

// the version gets delivers as an argument, because env might incompatible.
// this function must return false on version conflicts!
extern "C"
MRDOX_SYMBOL_EXPORT
void
MrDoxMain(
    const clang::mrdox::PluginInfo & pluginInfo);

#endif //MRDOX_PLUGIN_HPP
