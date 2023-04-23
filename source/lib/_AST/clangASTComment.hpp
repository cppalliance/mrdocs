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

#ifndef MRDOX_LIB_AST_CLANGASTCOMMENT_HPP
#define MRDOX_LIB_AST_CLANGASTCOMMENT_HPP

#include <mrdox/Platform.hpp>

// This is a workaround for the annoying
// warning in the current trunk of llvm

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054) // C5054: operator '+': deprecated between enumerations of different types
#endif

#include <clang/AST/Comment.h>
#include <clang/AST/CommentVisitor.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
