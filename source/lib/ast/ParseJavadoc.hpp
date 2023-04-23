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

#ifndef MRDOX_LIB_AST_PARSEJAVADOC_HPP
#define MRDOX_LIB_AST_PARSEJAVADOC_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/meta/Javadoc.hpp>
#include <clang/AST/Decl.h>
#include "clangASTComment.hpp"
#include <clang/AST/ASTContext.h>

namespace clang {
namespace mrdox {

/** Return a complete javadoc object for a raw comment.
*/
Javadoc
parseJavadoc(
    RawComment const* RC,
    ASTContext const& Ctx,
    Decl const* D);

} // mrdox
} // clang

#endif
