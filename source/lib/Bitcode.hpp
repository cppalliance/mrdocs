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

#ifndef MRDOX_SOURCE_BITCODE_HPP
#define MRDOX_SOURCE_BITCODE_HPP

#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <llvm/Bitstream/BitstreamWriter.h>
#include <memory>
#include <vector>

namespace clang {
namespace mrdox {

/** Write an Info variant to the bitstream.
*/
void
writeBitcode(
    Info const& I,
    llvm::BitstreamWriter& Stream);

/** Return an array of Info read from a bitstream.
*/
llvm::Expected<
    std::vector<std::unique_ptr<Info>>>
readBitcode(
    llvm::BitstreamCursor& Stream,
    Reporter& R);

} // mrdox
} // clang

#endif
