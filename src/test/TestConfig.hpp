// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//

#ifndef MRDOX_TEST_CONFIG_HPP
#define MRDOX_TEST_CONFIG_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdox {

struct TestConfig
{
  std::string cxxstd = "c++20";
  std::vector<std::string> compile_flags;
  bool should_fail = false;
  std::string heuristics = "unit test";

  MRDOX_DECL
  static
  Expected<std::vector<TestConfig>>
  loadForTest(
      std::string_view  dir,
      std::string_view  file);
};


}
}


#endif //MRDOX_TEST_CONFIG_HPP
