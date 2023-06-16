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

namespace clang {
namespace mrdox {

class ASTDiagnostics
{
public:
    void
    warning();
};

} // mrdox
} // clang

#endif
