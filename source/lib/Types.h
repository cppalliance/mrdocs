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

#ifndef MRDOX_TYPES_HPP
#define MRDOX_TYPES_HPP

#include <llvm/ADT/SmallString.h>

namespace clang {
namespace doc {
struct FunctionInfo;
} // doc
} // clang

//------------------------------------------------

namespace mrdox {

namespace doc = clang::doc;

/// The string used for unqualified names
using UnqualifiedName = llvm::SmallString<16>;

/// A list of zero or more functions
using FunctionInfos = std::vector<doc::FunctionInfo>;

} // mrdox

#endif
