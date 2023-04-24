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

#ifndef MRDOX_META_MEMBERTYPE_HPP
#define MRDOX_META_MEMBERTYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/FieldType.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <llvm/ADT/Optional.h>

namespace clang {
namespace mrdox {

// Info for member types.
struct MemberTypeInfo
    : FieldTypeInfo
    //, SymbolInfo
{
    // VFALCO This is public because 0 values in bitcode are not emitted...
    // Access level associated with this info (public, protected, private, none).
    // AS_public is set as default because the bitcode writer requires the enum
    // with value 0 to be used as the default.
    // (AS_public = 0, AS_protected = 1, AS_private = 2, AS_none = 3)
    AccessSpecifier Access = AccessSpecifier::AS_public;

    llvm::Optional<Javadoc> javadoc;

    //--------------------------------------------

    MemberTypeInfo() = default;
    
    MemberTypeInfo(
        TypeInfo const& TI,
        llvm::StringRef Name,
        AccessSpecifier Access)
        : FieldTypeInfo(TI, Name)
        , Access(Access)
    {
    }

    bool
    operator==(
        MemberTypeInfo const& Other) const
    {
        return
            std::tie(Type, Name, Access, javadoc) ==
            std::tie(Other.Type, Other.Name, Other.Access, Other.javadoc);
    }
};

} // mrdox
} // clang

#endif
