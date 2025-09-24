//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Debug.hpp>
#include <lib/Support/Radix.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Record.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <format>
#include <memory>

namespace clang {
namespace mrdocs {

void
debugEnableHeapChecking()
{
#if defined(_MSC_VER) && ! defined(NDEBUG)
    int flags = _CrtSetDbgFlag(
        _CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(flags);
#endif
}

} // mrdocs
} // clang

std::string
std::formatter<clang::mrdocs::Info>::toString(clang::mrdocs::Info const &i) {
  std::string str = std::format("Info: kind = {}", i.Kind);
  if (!i.Name.empty()) {
    str += std::format(", name = '{}'", i.Name);
  }
  str += std::format(", ID = {}", i.id);
  clang::mrdocs::SymbolID curParent = i.Parent;
  std::string namespaces;
  while (curParent) {
    namespaces += std::format("{}", curParent);
    curParent = i.Parent;
    if (curParent) {
      namespaces += "::";
    }
  }
  if (!namespaces.empty()) {
    str += std::format(", namespace = {}", namespaces);
  }
  return str;
}
