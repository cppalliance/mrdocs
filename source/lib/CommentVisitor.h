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

#ifndef MRDOX_COMMENT_VISITOR_HPP
#define MRDOX_COMMENT_VISITOR_HPP

#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/ASTContext.h>

namespace clang {
namespace mrdox {

/** Parse a FullComment
*/
void
parseComment(
    comments::FullComment const* c,
    ASTContext& ctx,
    Javadoc& javadoc,
    CommentInfo& ci,
    Reporter& R);

} // mrdox
} // clang

#endif
