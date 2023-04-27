//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_VARIABLE_HPP
#define MRDOX_METADATA_VARIABLE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbol.hpp>

namespace clang {
namespace mrdox {

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VariableInfo : SymbolInfo
{
    TypeInfo Type;

    explicit
    VariableInfo(
        SymbolID ID = SymbolID(),
        llvm::StringRef Name = llvm::StringRef())
        : SymbolInfo(InfoType::IT_variable, ID, Name)
    {
    }
};

} // mrdox
} // clang

#endif
