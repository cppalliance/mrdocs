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
    const DecltypeType* T,
    unsigned quals,
    bool pack)
{
    // KRYSTIAN TODO: support decltype in names
    // (e.g. within nested-name-specifiers).
}

void
NameInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const Type* T,
    unsigned quals,
    bool pack)
{
    if (getASTVisitor().checkSpecialNamespace(Result, NNS, nullptr))
    {
        return;
    }

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
    unsigned quals,
    bool pack)
{
    ASTVisitor& V = getASTVisitor();
    if (V.checkSpecialNamespace(Result, NNS, nullptr))
    {
        return;
    }

    if(TArgs)
    {
        auto I = std::make_unique<SpecializationNameInfo>();
        if (II)
        {
            I->Name = II->getName();
        }
        V.populate(I->TemplateArgs, *TArgs);
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
        Result->Prefix = V.toNameInfo(NNS);
    }
}

void
NameInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const NamedDecl* D,
    std::optional<ArrayRef<TemplateArgument>> const& TArgs,
    unsigned quals,
    bool pack)
{
    ASTVisitor& V = getASTVisitor();
    if (V.checkSpecialNamespace(Result, NNS, D))
    {
        return;
    }

    const IdentifierInfo* II = D->getIdentifier();
    if(TArgs)
    {
        auto I = std::make_unique<SpecializationNameInfo>();
        if (II)
        {
            I->Name = II->getName();
        }
        V.upsertDependency(getInstantiatedFrom(D), I->id);
        V.populate(I->TemplateArgs, *TArgs);
        Result = std::move(I);
    }
    else
    {
        auto I = std::make_unique<NameInfo>();
        if (II)
        {
            I->Name = II->getName();
        }
        V.upsertDependency(getInstantiatedFrom(D), I->id);
        Result = std::move(I);
    }
    if (NNS)
    {
        Result->Prefix = V.toNameInfo(NNS);
    }
}


} // clang::mrdocs
