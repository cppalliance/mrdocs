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

#ifndef MRDOX_META_TYPEDEF_HPP
#define MRDOX_META_TYPEDEF_HPP

#include <mrdox/meta/Symbol.hpp>
#include <mrdox/meta/Type.hpp>

namespace clang {
namespace mrdox {

// Info for typedef and using statements.
struct TypedefInfo : SymbolInfo
{
    static constexpr InfoType type_id = InfoType::IT_typedef;

    TypedefInfo(
        SymbolID USR = SymbolID())
        : SymbolInfo(InfoType::IT_typedef, USR)
    {
    }

    void merge(TypedefInfo&& I);

    TypeInfo Underlying;

    // Inidicates if this is a new C++ "using"-style typedef:
    //   using MyVector = std::vector<int>
    // False means it's a C-style typedef:
    //   typedef std::vector<int> MyVector;
    bool IsUsing = false;
};

} // mrdox
} // clang

#endif
