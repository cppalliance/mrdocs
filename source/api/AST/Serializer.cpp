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

#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/Debug.hpp>
#include <mrdox/Metadata.hpp>
#include <clang/AST/DeclFriend.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/SHA1.h>
#include <cassert>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

namespace clang {
namespace mrdox {


} // mrdox
} // clang
