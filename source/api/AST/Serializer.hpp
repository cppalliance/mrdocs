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

#ifndef MRDOX_API_AST_SERIALIZER_HPP
#define MRDOX_API_AST_SERIALIZER_HPP

#include <mrdox/Platform.hpp>
#include "clangASTComment.hpp"
#include "Bitcode.hpp"
#include "ConfigImpl.hpp"
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/DeclFriend.h>
#include <string>
#include <utility>
#include <vector>

/*  The serialization algorithm takes as input a
    typed AST node, and produces as output a pair
    of one or two Bitcode objects. A bitcode
    contains the metadata for a given symbol ID,
    serialized as bitcode.
*/

namespace clang {
namespace mrdox {

} // mrdox
} // clang

#endif

