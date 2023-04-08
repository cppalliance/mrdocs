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

#ifndef MRDOX_JAD_SYMBOL_HPP
#define MRDOX_JAD_SYMBOL_HPP

#include "Info.hpp"
#include "Location.hpp"
#include "llvm/ADT/Optional.h"

namespace clang {
namespace mrdox {

// Info for symbols.
struct SymbolInfo
    : public Info
{
    explicit
    SymbolInfo(
        InfoType IT,
        SymbolID USR = SymbolID(),
        llvm::StringRef Name = llvm::StringRef(),
        llvm::StringRef Path = llvm::StringRef())
        : Info(IT, USR, Name, Path)
    {
    }

    void merge(SymbolInfo&& I);

    llvm::Optional<Location> DefLoc;     // Location where this decl is defined.
    llvm::SmallVector<Location, 2> Loc; // Locations where this decl is declared.
};

} // mrdox
} // clang

#endif
