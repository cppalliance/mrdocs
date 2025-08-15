//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/AST/NameInfoBuilder.hpp"

namespace clang::mrdocs {

void
NameInfoBuilder::
buildDecltype(
    DecltypeType const*,
    unsigned,
    bool)
{
    // KRYSTIAN TODO: support decltype in names
    // (e.g. within nested-name-specifiers).
}

void
NameInfoBuilder::
buildTerminal(
    Type const* T,
    unsigned,
    bool)
{
    Result = Polymorphic<NameInfo>();
    Result->Name = getASTVisitor().toString(T);
}

void
NameInfoBuilder::
buildTerminal(
    NestedNameSpecifier NNS,
    IdentifierInfo const* II,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned,
    bool)
{
    if (TArgs)
    {
        Result =
            Polymorphic<NameInfo>(std::in_place_type<SpecializationNameInfo>);
        if (II)
        {
            Result->Name = II->getName();
        }
        getASTVisitor().populate(
            static_cast<SpecializationNameInfo &>(*Result).TemplateArgs, *TArgs);
    }
    else
    {
        Result = Polymorphic<NameInfo>();
        if (II)
        {
            Result->Name = II->getName();
        }
    }
    if (NNS)
    {
        Result->Prefix = getASTVisitor().toNameInfo(NNS);
    }
}

void
NameInfoBuilder::
buildTerminal(
    NestedNameSpecifier NNS,
    NamedDecl const* D,
    std::optional<ArrayRef<TemplateArgument>> const& TArgs,
    unsigned,
    bool)
{
    // Look for the Info type. If this is a template specialization,
    // we look for the Info of the specialized record.
    Decl const* ID = decayToPrimaryTemplate(D);


    auto populateNameInfo = [&](NameInfo& Name, NamedDecl const* DU)
    {
        if(IdentifierInfo const* II = DU->getIdentifier())
        {
            Name.Name = II->getName();
        }
        if (Info const* I = getASTVisitor().findOrTraverse(const_cast<Decl*>(ID)))
        {
            Name.id = I->id;
        }
        if (NNS)
        {
            Name.Prefix = getASTVisitor().toNameInfo(NNS);
        }
    };

    if (!TArgs)
    {
        Result = Polymorphic<NameInfo>();
        populateNameInfo(*Result, D);
    }
    else
    {
        Result =
            Polymorphic<NameInfo>(std::in_place_type<SpecializationNameInfo>);
        populateNameInfo(*Result, D);
        getASTVisitor().populate(
            static_cast<SpecializationNameInfo &>(*Result).TemplateArgs, *TArgs);
    }
}

} // clang::mrdocs
