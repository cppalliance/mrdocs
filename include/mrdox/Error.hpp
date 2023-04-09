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

#ifndef MRDOX_ERROR_HPP
#define MRDOX_ERROR_HPP

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <source_location>

namespace clang {
namespace mrdox {

/** Return an Error for the given text and source location.
*/
[[nodiscard]]
llvm::Error
makeError(
    llvm::StringRef what,
    std::source_location loc =
        std::source_location::current());

} // mrdox
} // clang

#endif
