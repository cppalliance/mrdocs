//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Klemens D. Morgenstern
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_VERSION_HPP
#define MRDOCS_VERSION_HPP

#include <string_view>

namespace clang {
namespace mrdocs {

constexpr std::string_view project_version     = "1.0.0";
constexpr std::string_view project_name        = "MrDocs";
constexpr std::string_view project_description = "C++ Documentation Tool";

} // mrdocs
} // clang

#endif
