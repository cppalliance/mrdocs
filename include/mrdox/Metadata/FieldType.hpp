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

#ifndef MRDOX_METADATA_FIELDTYPE_HPP
#define MRDOX_METADATA_FIELDTYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <llvm/ADT/SmallString.h>
#include <utility>

namespace clang {
namespace mrdox {

// Info for field types.
struct FieldTypeInfo
    : public TypeInfo
{
    llvm::SmallString<16> Name; // Name associated with this info.

    // When used for function parameters, contains the string representing the
    // expression of the default value, if any.
    llvm::SmallString<16> DefaultValue;

    //--------------------------------------------

    FieldTypeInfo() = default;

    FieldTypeInfo(
        TypeInfo const& TI,
        llvm::StringRef Name = llvm::StringRef(),
        llvm::StringRef DefaultValue = llvm::StringRef())
        : TypeInfo(TI)
        , Name(Name)
        , DefaultValue(DefaultValue)
    {
    }

    bool
    operator==(
        FieldTypeInfo const& Other) const
    {
        return
            std::tie(Type, Name, DefaultValue) ==
            std::tie(Other.Type, Other.Name, Other.DefaultValue);
    }
};

} // mrdox
} // clang

#endif
