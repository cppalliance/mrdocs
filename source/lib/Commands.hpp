//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_COMMANDS_HPP
#define MRDOX_LIB_COMMANDS_HPP

#include <mrdox/Platform.hpp>

namespace clang {

class ASTContext;

namespace mrdox {

/** Initialize clang to recognize our custom comments.

    Safe to be called more than once, but
    not concurrently.
*/
void
initCustomCommentCommands(
    ASTContext& ctx);

} // mrdox
} // clang

#endif
