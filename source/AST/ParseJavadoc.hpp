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

#ifndef MRDOX_TOOL_AST_PARSEJAVADOC_HPP
#define MRDOX_TOOL_AST_PARSEJAVADOC_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Javadoc.hpp>

namespace clang {

class Decl;
class ASTContext;
class RawComment;

namespace mrdox {

/** Initialize clang to recognize our custom comments.

    Safe to be called more than once, but
    not concurrently.
*/
void
initCustomCommentCommands(
    ASTContext& ctx);

/** Return a complete javadoc object for a raw comment.
*/
Javadoc
parseJavadoc(
    RawComment const* RC,
    Decl const* D);

} // mrdox

} // clang

#endif
