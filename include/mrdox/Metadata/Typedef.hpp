//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_TYPEDEF_HPP
#define MRDOX_API_METADATA_TYPEDEF_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <memory>

namespace clang {
namespace mrdox {

// Info for typedef and using statements.
struct TypedefInfo
    : IsInfo<InfoKind::Typedef>
    , SymbolInfo
{
    friend class ASTVisitor;

    TypeInfo Underlying;

    // Indicates if this is a new C++ "using"-style typedef:
    //   using MyVector = std::vector<int>
    // False means it's a C-style typedef:
    //   typedef std::vector<int> MyVector;
    bool IsUsing = false;

    std::unique_ptr<TemplateInfo> Template;

    //--------------------------------------------

    explicit
    TypedefInfo(
        SymbolID ID = SymbolID::zero)
        : IsInfo(ID)
    {
    }

private:
    // KRYSTIAN NOTE: if Template is non-null,
    // then IsUsing *must* be set; should we do it here?
    explicit
    TypedefInfo(
        std::unique_ptr<TemplateInfo>&& T)
        : Template(std::move(T))
    {
    }
};

} // mrdox
} // clang

#endif
