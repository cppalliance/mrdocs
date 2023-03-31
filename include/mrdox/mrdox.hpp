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

#ifndef MRDOX_MRDOX_HPP
#define MRDOX_MRDOX_HPP

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include "llvm/Support/Error.h"

namespace clang {
namespace doc {

llvm::Expected<llvm::Twine>
renderXML(
    llvm::StringRef fileName);

} // doc
} // clang

#endif
