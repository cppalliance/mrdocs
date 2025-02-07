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
    NestedNameSpecifier const* NNS,
    Type const* T,
    unsigned,
    bool)
{
    NameInfo I;
    I.Name = getASTVisitor().toString(T);
    Result = std::move(I);
    if (NNS)
    {
        Result->Prefix = getASTVisitor().toNameInfo(NNS);
    }
}

void
NameInfoBuilder::
buildTerminal(
    NestedNameSpecifier const* NNS,
    IdentifierInfo const* II,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned,
    bool)
{
    if (TArgs)
    {
        auto I = MakePolymorphic<SpecializationNameInfo>();
        if (II)
        {
            I->Name = II->getName();
        }
        getASTVisitor().populate(I->TemplateArgs, *TArgs);
        Result = Polymorphic<NameInfo>(std::move(I));
    }
    else
    {
        auto I = MakePolymorphic<NameInfo>();
        if (II)
        {
            I->Name = II->getName();
        }
        Result = std::move(I);
    }
    if (NNS)
    {
        Result->Prefix = getASTVisitor().toNameInfo(NNS);
    }
}

void
NameInfoBuilder::
buildTerminal(
    NestedNameSpecifier const* NNS,
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

    Polymorphic<NameInfo> TI;
    if (!TArgs)
    {
        NameInfo Name;
        populateNameInfo(Name, D);
        TI = std::move(Name);
    }
    else
    {
        SpecializationNameInfo Name;
        populateNameInfo(Name, D);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
        TI = std::move(Name);
    }
    Result = std::move(TI);
}

} // clang::mrdocs
