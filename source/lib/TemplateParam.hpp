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

//
// This file defines the internal representations of different declaration
// types for the clang-doc tool.
//

#ifndef MRDOX_TEMPLATEPARAM_HPP
#define MRDOX_TEMPLATEPARAM_HPP

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>

namespace clang {
namespace mrdox {

// Represents one template parameter.
//
// This is a very simple serialization of the text of the source code of the
// template parameter. It is saved in a struct so there is a place to add the
// name and default values in the future if needed.
//
/** A template parameter.
*/
struct TemplateParamInfo
{
    TemplateParamInfo() = default;

    explicit
    TemplateParamInfo(
        NamedDecl const& ND);

    TemplateParamInfo(
        Decl const& D,
        TemplateArgument const &Arg);

    TemplateParamInfo(
        llvm::StringRef Contents)
        : Contents(Contents)
    {
    }

    // The literal contents of the code for that specifies this template parameter
    // for this declaration. Typical values will be "class T" and
    // "typename T = int".
    llvm::SmallString<16> Contents;
};

} // mrdox
} // clang

#endif
