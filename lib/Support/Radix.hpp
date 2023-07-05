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

#ifndef MRDOX_TOOL_SUPPORT_RADIX_HPP
#define MRDOX_TOOL_SUPPORT_RADIX_HPP

#include <mrdox/Platform.hpp>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

std::string
toBase64(std::string_view str);

llvm::StringRef
toBaseFN(
    llvm::SmallVectorImpl<char>& dest,
    llvm::ArrayRef<uint8_t> src);

std::string_view
toBase32(
    std::string& dest,
    std::string_view src);

std::string
toBase16(
    std::string_view str,
    bool lowercase = false);

} // mrdox
} // clang

#endif
