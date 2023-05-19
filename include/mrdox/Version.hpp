//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_VERSION_HPP
#define MRDOX_VERSION_HPP

#include <string_view>

namespace clang {
namespace mrdox {

constexpr std::string_view project_version     = "1.0.0";
constexpr std::string_view project_name        = "MrDox";
constexpr std::string_view project_description = "C++ Documentation Tool";

} // mrdox
} // clang

#endif
