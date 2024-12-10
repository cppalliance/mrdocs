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

#include "lib/AST/TypeInfoBuilder.hpp"

namespace clang::mrdocs {

void
TypeInfoBuilder::
buildPointer(const PointerType* T, unsigned quals)
{
    auto I = std::make_unique<PointerTypeInfo>();
    I->CVQualifiers = convertToQualifierKind(quals);
    *std::exchange(Inner, &I->PointeeType) = std::move(I);
}

void
TypeInfoBuilder::
buildLValueReference(const LValueReferenceType* T)
{
    auto I = std::make_unique<LValueReferenceTypeInfo>();
    *std::exchange(Inner, &I->PointeeType) = std::move(I);
}

void
TypeInfoBuilder::
buildRValueReference(const RValueReferenceType* T)
{
    auto I = std::make_unique<RValueReferenceTypeInfo>();
    *std::exchange(Inner, &I->PointeeType) = std::move(I);
}

void
TypeInfoBuilder::
buildMemberPointer(const MemberPointerType* T, unsigned quals)
{
    auto I = std::make_unique<MemberPointerTypeInfo>();
    I->CVQualifiers = convertToQualifierKind(quals);
    // do not set NNS because the parent type is *not*
    // a nested-name-specifier which qualifies the pointee type
    I->ParentType = getASTVisitor().toTypeInfo(
        QualType(T->getClass(), 0));
    *std::exchange(Inner, &I->PointeeType) = std::move(I);
}

void
TypeInfoBuilder::
buildArray(const ArrayType* T)
{
    auto I = std::make_unique<ArrayTypeInfo>();
    if(auto* CAT = dyn_cast<ConstantArrayType>(T))
    {
        getASTVisitor().populate(
            I->Bounds, CAT->getSizeExpr(), CAT->getSize());
    }
    else if(auto* DAT = dyn_cast<DependentSizedArrayType>(T))
    {
        getASTVisitor().populate(
            I->Bounds, DAT->getSizeExpr());
    }
    *std::exchange(Inner, &I->ElementType) = std::move(I);
}

void
TypeInfoBuilder::
populate(const FunctionType* T)
{
    auto* FPT = cast<FunctionProtoType>(T);
    auto I = std::make_unique<FunctionTypeInfo>();
    for(QualType PT : FPT->getParamTypes())
    {
        I->ParamTypes.emplace_back(
            getASTVisitor().toTypeInfo(PT));
    }
    I->RefQualifier = convertToReferenceKind(
        FPT->getRefQualifier());
    I->CVQualifiers = convertToQualifierKind(
        FPT->getMethodQuals().getFastQualifiers());
    I->IsVariadic = FPT->isVariadic();
    getASTVisitor().populate(I->ExceptionSpec, FPT);
    *std::exchange(Inner, &I->ReturnType) = std::move(I);
}

void
TypeInfoBuilder::
buildDecltype(
    const DecltypeType* T,
    unsigned quals,
    bool pack)
{
    auto I = std::make_unique<DecltypeTypeInfo>();
    getASTVisitor().populate(
        I->Operand, T->getUnderlyingExpr());
    I->CVQualifiers = convertToQualifierKind(quals);
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildAuto(
    const AutoType* T,
    unsigned const quals,
    bool const pack)
{
    auto I = std::make_unique<AutoTypeInfo>();
    I->CVQualifiers = convertToQualifierKind(quals);
    I->Keyword = convertToAutoKind(T->getKeyword());
    if(T->isConstrained())
    {
        std::optional<ArrayRef<TemplateArgument>> TArgs;
        if(auto Args = T->getTypeConstraintArguments();
            ! Args.empty())
        {
            TArgs.emplace(Args);
        }
        I->Constraint = getASTVisitor().toNameInfo(
            T->getTypeConstraintConcept(), TArgs);
        // Constraint->Prefix = getASTVisitor().buildNameInfo(
        //     cast<Decl>(CD->getDeclContext()));
    }
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}



void
TypeInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const Type* T,
    unsigned quals,
    bool pack)
{
    if(getASTVisitor().checkSpecialNamespace(*Inner, NNS, nullptr))
    {
        return;
    }

    auto I = std::make_unique<NamedTypeInfo>();
    I->CVQualifiers = convertToQualifierKind(quals);

    auto Name = std::make_unique<NameInfo>();
    Name->Name = getASTVisitor().toString(T);
    Name->Prefix = getASTVisitor().toNameInfo(NNS);
    I->Name = std::move(Name);
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const IdentifierInfo* II,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    ASTVisitor& V = getASTVisitor();
    if(V.checkSpecialNamespace(*Inner, NNS, nullptr))
    {
        return;
    }

    auto I = std::make_unique<NamedTypeInfo>();
    I->CVQualifiers = convertToQualifierKind(quals);

    if(TArgs)
    {
        auto Name = std::make_unique<SpecializationNameInfo>();
        if(II)
        {
            Name->Name = II->getName();
        }
        Name->Prefix = getASTVisitor().toNameInfo(NNS);
        V.populate(Name->TemplateArgs, *TArgs);
        I->Name = std::move(Name);
    }
    else
    {
        auto Name = std::make_unique<NameInfo>();
        if(II)
        {
            Name->Name = II->getName();
        }
        Name->Prefix = getASTVisitor().toNameInfo(NNS);
        I->Name = std::move(Name);
    }
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    const NestedNameSpecifier* NNS,
    const NamedDecl* D,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    ASTVisitor& V = getASTVisitor();
    if(V.checkSpecialNamespace(*Inner, NNS, D))
    {
        return;
    }

    auto I = std::make_unique<NamedTypeInfo>();
    I->CVQualifiers = convertToQualifierKind(quals);

    if(TArgs)
    {
        auto Name = std::make_unique<SpecializationNameInfo>();
        if(const IdentifierInfo* II = D->getIdentifier())
        {
            Name->Name = II->getName();
        }
        V.upsertDependency(getInstantiatedFrom(D), Name->id);
        if(NNS)
        {
            Name->Prefix = V.toNameInfo(NNS);
        }

        V.populate(Name->TemplateArgs, *TArgs);
        I->Name = std::move(Name);
    }
    else
    {
        auto Name = std::make_unique<NameInfo>();
        if(const IdentifierInfo* II = D->getIdentifier())
        {
            Name->Name = II->getName();
        }
        V.upsertDependency(getInstantiatedFrom(D), Name->id);
        if(NNS)
        {
            Name->Prefix = V.toNameInfo(NNS);
        }
        I->Name = std::move(Name);
    }
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}

} // clang::mrdocs
