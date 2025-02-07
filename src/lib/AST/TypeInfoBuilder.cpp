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
buildPointer(PointerType const*, unsigned quals)
{
    PointerTypeInfo I;
    I.CVQualifiers = toQualifierKind(quals);
    *Inner = std::move(I);
    Inner = &get<PointerTypeInfo&>(*Inner).PointeeType;
}

void
TypeInfoBuilder::
buildLValueReference(LValueReferenceType const*)
{
    LValueReferenceTypeInfo I;
    *Inner = std::move(I);
    Inner = &get<LValueReferenceTypeInfo&>(*Inner).PointeeType;
}

void
TypeInfoBuilder::
buildRValueReference(RValueReferenceType const*)
{
    RValueReferenceTypeInfo I;
    *Inner = std::move(I);
    Inner = &get<RValueReferenceTypeInfo&>(*Inner).PointeeType;
}

void
TypeInfoBuilder::
buildMemberPointer(
    MemberPointerType const* T,
    unsigned const quals)
{
    MemberPointerTypeInfo I;
    I.CVQualifiers = toQualifierKind(quals);
    // do not set NNS because the parent type is *not*
    // a nested-name-specifier which qualifies the pointee type
    I.ParentType = getASTVisitor().toTypeInfo(
        QualType(T->getClass(), 0));
    *Inner = std::move(I);
    Inner = &get<MemberPointerTypeInfo&>(*Inner).PointeeType;
}

void
TypeInfoBuilder::
buildArray(ArrayType const* T)
{
    ArrayTypeInfo I;
    if (auto* CAT = dyn_cast<ConstantArrayType>(T))
    {
        getASTVisitor().populate(I.Bounds, CAT->getSizeExpr(), CAT->getSize());
    }
    else if (auto* DAT = dyn_cast<DependentSizedArrayType>(T))
    {
        getASTVisitor().populate(I.Bounds, DAT->getSizeExpr());
    }
    *Inner = std::move(I);
    Inner = &get<ArrayTypeInfo&>(*Inner).ElementType;
}

void
TypeInfoBuilder::
populate(FunctionType const* T)
{
    auto* FPT = cast<FunctionProtoType>(T);
    FunctionTypeInfo I;
    for (QualType const PT : FPT->getParamTypes())
    {
        I.ParamTypes.emplace_back(getASTVisitor().toTypeInfo(PT));
    }
    I.RefQualifier = toReferenceKind(
        FPT->getRefQualifier());
    I.CVQualifiers = toQualifierKind(
        FPT->getMethodQuals().getFastQualifiers());
    I.IsVariadic = FPT->isVariadic();
    getASTVisitor().populate(I.ExceptionSpec, FPT);
    *Inner = std::move(I);
    Inner = &get<FunctionTypeInfo&>(*Inner).ReturnType;
}

void
TypeInfoBuilder::
buildDecltype(
    DecltypeType const* T,
    unsigned quals,
    bool pack)
{
    DecltypeTypeInfo I;;
    getASTVisitor().populate(
        I.Operand, T->getUnderlyingExpr());
    I.CVQualifiers = toQualifierKind(quals);
    I.Constraints = this->Constraints;
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildAuto(
    AutoType const* T,
    unsigned const quals,
    bool const pack)
{
    AutoTypeInfo I;
    I.CVQualifiers = toQualifierKind(quals);
    I.Keyword = convertToAutoKind(T->getKeyword());
    if(T->isConstrained())
    {
        std::optional<ArrayRef<TemplateArgument>> TArgs;
        if(auto Args = T->getTypeConstraintArguments();
            ! Args.empty())
        {
            TArgs.emplace(Args);
        }
        I.Constraint = getASTVisitor().toNameInfo(
            T->getTypeConstraintConcept(), TArgs);
        // Constraint->Prefix = getASTVisitor().buildNameInfo(
        //     cast<Decl>(CD->getDeclContext()));
    }
    I.Constraints = this->Constraints;
    *Inner = std::move(I);
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    NestedNameSpecifier const* NNS,
    Type const* T,
    unsigned quals,
    bool pack)
{
    NamedTypeInfo TI;
    TI.CVQualifiers = toQualifierKind(quals);
    TI.Name = MakePolymorphic<NameInfo>();
    TI.Name->Name = getASTVisitor().toString(T);
    TI.Name->Prefix = getASTVisitor().toNameInfo(NNS);
    TI.Constraints = this->Constraints;
    *Inner = std::move(TI);
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    NestedNameSpecifier const* NNS,
    IdentifierInfo const* II,
    std::optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    NamedTypeInfo I;
    I.CVQualifiers = toQualifierKind(quals);

    if (TArgs)
    {
        SpecializationNameInfo Name;
        if(II)
        {
            Name.Name = II->getName();
        }
        Name.Prefix = getASTVisitor().toNameInfo(NNS);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
        I.Name = std::move(Name);
    }
    else
    {
        NameInfo Name;
        if(II)
        {
            Name.Name = II->getName();
        }
        Name.Prefix = getASTVisitor().toNameInfo(NNS);
        I.Name = std::move(Name);
    }
    I.Constraints = this->Constraints;
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

    NamedTypeInfo TI;
    TI.CVQualifiers = toQualifierKind(quals);

    auto populateNameInfo = [&](NameInfo& Name, NamedDecl* D)
    {
        if(IdentifierInfo const* II = D->getIdentifier())
        {
            Name.Name = II->getName();
        }
        if (Info const* I = getASTVisitor().findOrTraverse(const_cast<Decl*>(ID)))
        {
            Name.id = I->id;
        }
        if(NNS)
        {
            Name.Prefix = getASTVisitor().toNameInfo(NNS);
        }
    };

    if (!TArgs)
    {
        NameInfo Name;
        populateNameInfo(Name, D);
        TI.Name = std::move(Name);
    }
    else
    {
        SpecializationNameInfo Name;
        populateNameInfo(Name, D);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
        TI.Name = std::move(Name);
    }
    TI.Constraints = this->Constraints;
    *Inner = std::move(TI);
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

} // clang::mrdocs
