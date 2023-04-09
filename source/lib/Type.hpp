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

#ifndef MRDOX_TYPE_HPP
#define MRDOX_TYPE_HPP

#include "Reference.hpp"

namespace clang {
namespace mrdox {

// A base struct for TypeInfos
struct TypeInfo
{
    TypeInfo() = default;

    TypeInfo(Reference const& R)
        : Type(R)
    {
    }

    // Convenience constructor for when there is no symbol ID or info type
    // (normally used for built-in types in tests).
    TypeInfo(
        llvm::StringRef Name,
        llvm::StringRef Path = llvm::StringRef())
        : Type(
            EmptySID,
            Name,
            InfoType::IT_default,
            Path)
    {
    }

    bool operator==(TypeInfo const& Other) const
    {
        return Type == Other.Type;
    }

    Reference Type; // Referenced type in this info.
};

} // mrdox
} // clang

#endif
