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

#ifndef MRDOX_JAVADOC_HPP
#define MRDOX_JAVADOC_HPP

#include <llvm/ADT/SmallString.h>
#include <vector>
#include <string>

namespace clang {
namespace doc {

/** A single verbatim block.
*/
struct VerbatimBlock
{
    std::string text;
};

/** A complete javadoc attached to a declaration
*/
struct Javadoc
{
    /** The brief description.
    */
    llvm::SmallString<32> brief;

    /** The detailed description.
    */
    //llvm::SmallString<32> desc; // asciidoc
    std::string desc; // asciidoc
};

} // doc
} // clang

#endif
