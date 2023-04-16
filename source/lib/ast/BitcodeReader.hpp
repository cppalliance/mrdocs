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

//
// This file implements a reader for parsing the clang-doc internal
// representation from LLVM bitcode. The reader takes in a stream of bits and
// generates the set of infos that it represents.
//

#ifndef MRDOX_SOURCE_AST_BITCODEREADER_HPP
#define MRDOX_SOURCE_AST_BITCODEREADER_HPP

#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <memory>
#include <vector>

namespace clang {
namespace mrdox {

/** Return an array of Info read from a Bitstream.
*/
llvm::Expected<
    std::vector<std::unique_ptr<Info>>>
readBitcode(
    llvm::BitstreamCursor& Stream,
    Reporter& R);

} // mrdox
} // clang

#endif
