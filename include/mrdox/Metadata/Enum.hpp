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

#ifndef MRDOX_META_ENUM_HPP
#define MRDOX_META_ENUM_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <utility>

namespace clang {
namespace mrdox {

// Information for a single possible value of an enumeration.
struct EnumValueInfo
{
    explicit
    EnumValueInfo(
        llvm::StringRef Name = llvm::StringRef(),
        llvm::StringRef Value = llvm::StringRef("0"),
        llvm::StringRef ValueExpr = llvm::StringRef())
        : Name(Name)
        , Value(Value)
        , ValueExpr(ValueExpr)
    {
    }

    bool operator==(EnumValueInfo const& Other) const
    {
        return
            std::tie(Name, Value, ValueExpr) ==
            std::tie(Other.Name, Other.Value, Other.ValueExpr);
    }

    llvm::SmallString<16> Name;

    // The computed value of the enumeration constant. This could be the result of
    // evaluating the ValueExpr, or it could be automatically generated according
    // to C rules.
    llvm::SmallString<16> Value;

    // Stores the user-supplied initialization expression for this enumeration
    // constant. This will be empty for implicit enumeration values.
    llvm::SmallString<16> ValueExpr;
};

//------------------------------------------------

// TODO: Expand to allow for documenting templating.
// Info for types.
struct EnumInfo : SymbolInfo
{
    // Indicates whether this enum is scoped (e.g. enum class).
    bool Scoped = false;

    // Set to nonempty to the type when this is an explicitly typed enum. For
    //   enum Foo : short { ... };
    // this will be "short".
    llvm::Optional<TypeInfo> BaseType;

    llvm::SmallVector<EnumValueInfo, 4> Members; // List of enum members.

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_enum;

    EnumInfo()
        : SymbolInfo(InfoType::IT_enum)
    {
    }

    explicit
    EnumInfo(
        SymbolID id_)
        : SymbolInfo(InfoType::IT_enum, id_)
    {
    }

    void merge(EnumInfo&& I);
};

} // mrdox
} // clang

#endif
