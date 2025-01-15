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
    const DecltypeType*,
    unsigned,
    bool)
{
    // KRYSTIAN TODO: support decltype in names
    // (e.g. within nested-name-specifiers).
}

void
NameInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const Type* T,
    unsigned,
    bool)
{
    auto I = std::make_unique<NameInfo>();
    I->Name = getASTVisitor().toString(T);
    Result = std::move(I);
    if (NNS)
    {
        Result->Prefix = getASTVisitor().toNameInfo(NNS);
    }
}

void
NameInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const IdentifierInfo* II,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned,
    bool)
{
    if(TArgs)
    {
        auto I = std::make_unique<SpecializationNameInfo>();
        if (II)
        {
            I->Name = II->getName();
        }
        getASTVisitor().populate(I->TemplateArgs, *TArgs);
        Result = std::move(I);
    }
    else
    {
        auto I = std::make_unique<NameInfo>();
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
    const NestedNameSpecifier* NNS,
    const NamedDecl* D,
    std::optional<ArrayRef<TemplateArgument>> const& TArgs,
    unsigned,
    bool)
{
    // Look for the Info type. If this is a template specialization,
    // we look for the Info of the specialized record.
    Decl const* ID = decayToPrimaryTemplate(D);

    auto TI = std::make_unique<NameInfo>();

    auto populateNameInfo = [&](NameInfo* Name, NamedDecl const* D)
    {
        if(const IdentifierInfo* II = D->getIdentifier())
        {
            Name->Name = II->getName();
        }
        if (Info const* I = getASTVisitor().findOrTraverse(const_cast<Decl*>(ID)))
        {
            Name->id = I->id;
        }
        if(NNS)
        {
            Name->Prefix = getASTVisitor().toNameInfo(NNS);
        }
    };

    if (!TArgs)
    {
        populateNameInfo(TI.get(), D);
    }
    else
    {
        auto Name = std::make_unique<SpecializationNameInfo>();
        populateNameInfo(Name.get(), D);
        getASTVisitor().populate(Name->TemplateArgs, *TArgs);
        TI = std::move(Name);
    }
    Result = std::move(TI);
}

} // clang::mrdocs
