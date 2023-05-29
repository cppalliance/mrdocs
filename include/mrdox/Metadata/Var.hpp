//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_VARIABLE_HPP
#define MRDOX_METADATA_VARIABLE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/BitField.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <clang/Basic/Specifiers.h>
#include <memory>

namespace clang {
namespace mrdox {

union VarFlags0
{
    BitFieldFullValue raw;

    BitField<0, 3, StorageClass> storageClass;
};

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VarInfo
    : SymbolInfo
{
    friend class ASTVisitor;

    /** The type of the variable */
    TypeInfo Type;

    VarFlags0 specs{.raw={0}};

    std::unique_ptr<TemplateInfo> Template;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_variable;

    explicit
    VarInfo(
        SymbolID ID = SymbolID::zero,
        llvm::StringRef Name = llvm::StringRef())
        : SymbolInfo(InfoType::IT_variable, ID, Name)
    {
    }

private:
    explicit
    VarInfo(
        std::unique_ptr<TemplateInfo>&& T)
        : SymbolInfo(InfoType::IT_variable)
        , Template(std::move(T))
    {

    }
};

} // mrdox
} // clang

#endif
