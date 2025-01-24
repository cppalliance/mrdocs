//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ENUM_HPP
#define MRDOCS_API_METADATA_ENUM_HPP

#include <mrdocs/ADT/PolymorphicValue.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Info/Scope.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

struct EnumInfo final
    : InfoCommonBase<InfoKind::Enum>
    , SourceInfo
    , ScopeInfo
{
    // Indicates whether this enum is scoped (e.g. enum class).
    bool Scoped = false;

    // Set too nonempty to the type when this is an explicitly typed enum. For
    //   enum Foo : short { ... };
    // this will be "short".
    PolymorphicValue<TypeInfo> UnderlyingType;

    //--------------------------------------------

    explicit EnumInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // clang::mrdocs

#endif
