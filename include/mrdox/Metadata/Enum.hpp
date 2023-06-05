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
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace clang {
namespace mrdox {

// FIXME: this does not store javadocs...
// Information for a single possible value of an enumeration.
struct EnumValueInfo
{
    std::string Name;

    // The computed value of the enumeration constant. This could be the result of
    // evaluating the ValueExpr, or it could be automatically generated according
    // to C rules.
    std::string Value;

    // Stores the user-supplied initialization expression for this enumeration
    // constant. This will be empty for implicit enumeration values.
    std::string ValueExpr;

    //--------------------------------------------

    explicit
    EnumValueInfo(
        std::string_view Name = "",
        std::string_view Value = "0",
        std::string_view ValueExpr = "")
        : Name(Name)
        , Value(Value)
        , ValueExpr(ValueExpr)
    {
    }

#if 0
    // VFALCO What was this for?
    bool operator==(EnumValueInfo const& Other) const
    {
        return
            std::tie(Name, Value, ValueExpr) ==
            std::tie(Other.Name, Other.Value, Other.ValueExpr);
    }
#endif
};

//------------------------------------------------

// TODO: Expand to allow for documenting templating.
// Info for types.
struct EnumInfo
    : SymbolInfo
{
    // Indicates whether this enum is scoped (e.g. enum class).
    bool Scoped = false;

    // Set to nonempty to the type when this is an explicitly typed enum. For
    //   enum Foo : short { ... };
    // this will be "short".
    std::optional<TypeInfo> BaseType;

    // Enumeration members.
    std::vector<EnumValueInfo> Members;

    //--------------------------------------------

    static constexpr InfoKind kind_id = InfoKind::Enum;

    explicit
    EnumInfo(
        SymbolID ID = SymbolID::zero)
        : SymbolInfo(InfoKind::Enum, ID)
    {
    }
};

} // mrdox
} // clang

#endif
