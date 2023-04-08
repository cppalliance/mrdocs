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

#ifndef MRDOX_XML_BASE64_HPP
#define MRDOX_XML_BASE64_HPP

#include <array>
#include <cstdint>
#include <string>

namespace clang {
namespace mrdox {

std::string toBase64(std::array<uint8_t, 20> const& v);

} // mrdox
} // clang

#endif
