//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Commands.hpp"
#include <clang/AST/ASTContext.h>
#include <clang/AST/CommentCommandTraits.h>

namespace clang {
namespace mrdox {

void
initCustomCommentCommands(ASTContext& context)
{
    auto& traits = context.getCommentCommandTraits();

#if 0
    {
        auto cmd = traits.registerBlockCommand("mrdox");
    }
#endif

    (void)traits;
}

} // mrdox
} // clang
