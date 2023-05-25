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

#ifndef MRDOX_METADATA_MEMBERTYPE_HPP
#define MRDOX_METADATA_MEMBERTYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Access.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/FieldType.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <llvm/ADT/Optional.h>

namespace clang {
namespace mrdox {

/** A data member for a class.
*/
struct MemberTypeInfo
    : FieldInfo
    //, SymbolInfo
{
    Access access = Access::Public;

    llvm::Optional<Javadoc> javadoc;

    //--------------------------------------------

    MemberTypeInfo() = default;
    
    MemberTypeInfo(
        TypeInfo const& TI,
        llvm::StringRef Name,
        Access access_)
        : FieldInfo(TI, Name)
        , access(access_)
    {
    }

#if 0
    // VFALCO What was this for?
    bool
    operator==(
        MemberTypeInfo const& Other) const
    {
        return
            std::tie(Type, Name, Access, javadoc) ==
            std::tie(Other.Type, Other.Name, Other.Access, Other.javadoc);
    }
#endif
};

} // mrdox
} // clang

#endif
