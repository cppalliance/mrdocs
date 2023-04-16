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

#ifndef MRDOX_SOURCE_PARSEJAVADOC_HPP
#define MRDOX_SOURCE_PARSEJAVADOC_HPP

#include <mrdox/meta/Javadoc.hpp>

namespace clang {

class ASTContext;
class Decl;
class RawComment;

namespace mrdox {

struct Javadoc;

void
dumpJavadoc(
    Javadoc const& jd);

Javadoc
parseJavadoc(
    RawComment const* RC,
    ASTContext const& Ctx,
    Decl const* D);

} // mrdox
} // clang

#endif
