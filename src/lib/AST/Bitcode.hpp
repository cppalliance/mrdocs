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

#ifndef MRDOX_LIB_AST_BITCODE_HPP
#define MRDOX_LIB_AST_BITCODE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <memory>
#include <vector>

namespace clang {
namespace mrdox {

/** Return the serialized bitcode for a metadata node.
*/
llvm::SmallString<0>
writeBitcode(Info const& I);

/** Return an array of Info read from a bitstream.
*/
mrdox::Expected<std::vector<std::unique_ptr<Info>>>
readBitcode(llvm::StringRef bitcode);

} // mrdox
} // clang

#endif
