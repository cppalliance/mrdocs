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
    I->CVQualifiers = toQualifierKind(quals);
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
    I->CVQualifiers = toQualifierKind(quals);
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
    I->RefQualifier = toReferenceKind(
        FPT->getRefQualifier());
    I->CVQualifiers = toQualifierKind(
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
    I->CVQualifiers = toQualifierKind(quals);
    I->Constraints = this->Constraints;
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
    I->CVQualifiers = toQualifierKind(quals);
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
    I->Constraints = this->Constraints;
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
    auto TI = std::make_unique<NamedTypeInfo>();
    TI->CVQualifiers = toQualifierKind(quals);
    TI->Name = std::make_unique<NameInfo>();
    TI->Name->Name = getASTVisitor().toString(T);
    TI->Name->Prefix = getASTVisitor().toNameInfo(NNS);
    TI->Constraints = this->Constraints;
    *Inner = std::move(TI);
    Result->Constraints = this->Constraints;
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
    auto I = std::make_unique<NamedTypeInfo>();
    I->CVQualifiers = toQualifierKind(quals);

    if(TArgs)
    {
        auto Name = std::make_unique<SpecializationNameInfo>();
        if(II)
        {
            Name->Name = II->getName();
        }
        Name->Prefix = getASTVisitor().toNameInfo(NNS);
        getASTVisitor().populate(Name->TemplateArgs, *TArgs);
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
    I->Constraints = this->Constraints;
    *Inner = std::move(I);
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    NestedNameSpecifier const* NNS,
    NamedDecl* D,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    MRDOCS_SYMBOL_TRACE(NNS, getASTVisitor().context_);
    MRDOCS_SYMBOL_TRACE(D, getASTVisitor().context_);
    MRDOCS_SYMBOL_TRACE(TArgs, getASTVisitor().context_);

    // Look for the Info type. If this is a template specialization,
    // we look for the Info of the specialized record.
    Decl const* ID = decayToPrimaryTemplate(D);
    MRDOCS_SYMBOL_TRACE(ID, getASTVisitor().context_);

    auto TI = std::make_unique<NamedTypeInfo>();
    TI->CVQualifiers = toQualifierKind(quals);

    auto populateNameInfo = [&](NameInfo* Name, NamedDecl* D)
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
        TI->Name = std::make_unique<NameInfo>();
        populateNameInfo(TI->Name.get(), D);
    }
    else
    {
        auto Name = std::make_unique<SpecializationNameInfo>();
        populateNameInfo(Name.get(), D);
        getASTVisitor().populate(Name->TemplateArgs, *TArgs);
        TI->Name = std::move(Name);
    }
    TI->Constraints = this->Constraints;
    *Inner = std::move(TI);
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

} // clang::mrdocs
