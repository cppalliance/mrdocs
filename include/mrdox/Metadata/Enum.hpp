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

#ifndef MRDOX_API_METADATA_ENUM_HPP
#define MRDOX_API_METADATA_ENUM_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Expression.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/Source.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace clang {
namespace mrdox {

// Information for a single possible value of an enumeration.
struct EnumValueInfo
{
    std::string Name;

    /** The initializer expression, if any */
    ConstantExprInfo<std::uint64_t> Initializer;

    /** The documentation for the value, if any.
    */
    std::unique_ptr<Javadoc> javadoc;

    //--------------------------------------------

    explicit
    EnumValueInfo(
        std::string_view Name = "")
        : Name(Name)
    {
    }
};

//------------------------------------------------

// TODO: Expand to allow for documenting templating.
// Info for types.
struct EnumInfo
    : IsInfo<InfoKind::Enum>
    , SourceInfo
{
    // Indicates whether this enum is scoped (e.g. enum class).
    bool Scoped = false;

    // Set to nonempty to the type when this is an explicitly typed enum. For
    //   enum Foo : short { ... };
    // this will be "short".
    std::unique_ptr<TypeInfo> UnderlyingType;

    // Enumeration members.
    std::vector<EnumValueInfo> Members;

    //--------------------------------------------

    explicit
    EnumInfo(
        SymbolID ID = SymbolID::zero)
        : IsInfo(ID)
    {
    }
};

} // mrdox
} // clang

#endif
