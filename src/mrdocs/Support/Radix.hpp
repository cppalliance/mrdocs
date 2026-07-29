//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_RADIX_HPP
#define MRDOCS_LIB_SUPPORT_RADIX_HPP

#include <mrdocs/Platform.hpp>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace mrdocs {

std::string
toBase64(std::string_view str);

/** Encode bytes with the Bitcoin base58 alphabet.

    Base58 is compact like base64 but avoids `0`, `O`, `I`, `l`, and the
    non-alphanumeric `+`/`/`/`=`, so the result is easy to copy, paste, and
    match with a regular expression. Leading zero bytes map to leading `1`s.
*/
std::string
toBase58(std::string_view str);

llvm::StringRef
toBaseFN(
    llvm::SmallVectorImpl<char>& dest,
    llvm::ArrayRef<uint8_t> src);

#if 0
std::string_view
toBase32(
    std::string& dest,
    std::string_view src);
#endif

std::string
toBase16(
    std::string_view str,
    bool lowercase = false);

} // mrdocs

#endif
