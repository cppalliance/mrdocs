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

#ifndef MRDOX_API_METADATA_VARIABLE_HPP
#define MRDOX_API_METADATA_VARIABLE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/BitField.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <clang/Basic/Specifiers.h>
#include <memory>

namespace clang {
namespace mrdox {

union VariableFlags0
{
    BitFieldFullValue raw;

    BitField<0, 3, StorageClass> storageClass;
};

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VariableInfo
    : IsInfo<InfoKind::Variable>
    , SymbolInfo
{
    friend class ASTVisitor;

    /** The type of the variable */
    TypeInfo Type;

    VariableFlags0 specs{.raw={0}};

    std::unique_ptr<TemplateInfo> Template;

    //--------------------------------------------

    explicit
    VariableInfo(
        SymbolID ID = SymbolID::zero)
        : IsInfo(ID)
    {
    }

private:
    explicit
    VariableInfo(
        std::unique_ptr<TemplateInfo>&& T)
        : Template(std::move(T))
    {

    }
};

} // mrdox
} // clang

#endif
