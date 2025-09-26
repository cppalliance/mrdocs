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

#include <lib/AST/TypeInfoBuilder.hpp>

namespace clang::mrdocs {

void
TypeInfoBuilder::
buildPointer(PointerType const*, unsigned quals)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<PointerTypeInfo>);
    auto &I = dynamic_cast<PointerTypeInfo &>(**Inner);
    I.IsConst = quals & Qualifiers::Const;
    I.IsVolatile = quals & Qualifiers::Volatile;
    Inner = &I.PointeeType;
}

void
TypeInfoBuilder::
buildLValueReference(LValueReferenceType const*)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<LValueReferenceTypeInfo>);
    Inner = &dynamic_cast<LValueReferenceTypeInfo &>(**Inner).PointeeType;
}

void
TypeInfoBuilder::
buildRValueReference(RValueReferenceType const*)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<RValueReferenceTypeInfo>);
    Inner = &dynamic_cast<RValueReferenceTypeInfo &>(**Inner).PointeeType;
}

void
TypeInfoBuilder::
buildMemberPointer(
    MemberPointerType const* T,
    unsigned const quals)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<MemberPointerTypeInfo>);
    auto &I = dynamic_cast<MemberPointerTypeInfo &>(**Inner);
    I.IsConst = quals & Qualifiers::Const;
    I.IsVolatile = quals & Qualifiers::Volatile;
    // do not set NNS because the parent type is *not*
    // a nested-name-specifier which qualifies the pointee type
    I.ParentType =
        getASTVisitor().toTypeInfo(QualType(T->getQualifier().getAsType(), 0));
    Inner = &I.PointeeType;
}

void
TypeInfoBuilder::
buildArray(ArrayType const* T)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<ArrayTypeInfo>);
    auto &I = dynamic_cast<ArrayTypeInfo &>(**Inner);
    if (auto* CAT = dyn_cast<ConstantArrayType>(T))
    {
        getASTVisitor().populate(I.Bounds, CAT->getSizeExpr(), CAT->getSize());
    }
    else if (auto* DAT = dyn_cast<DependentSizedArrayType>(T))
    {
        getASTVisitor().populate(I.Bounds, DAT->getSizeExpr());
    }
    Inner = &I.ElementType;
}

void
TypeInfoBuilder::
populate(FunctionType const* T)
{
    auto* FPT = cast<FunctionProtoType>(T);
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<FunctionTypeInfo>);
    auto &I = dynamic_cast<FunctionTypeInfo &>(**Inner);
    for (QualType const PT : FPT->getParamTypes())
    {
        I.ParamTypes.emplace_back(getASTVisitor().toTypeInfo(PT));
    }
    I.RefQualifier = toReferenceKind(
        FPT->getRefQualifier());
    unsigned const quals = FPT->getMethodQuals().getFastQualifiers();
    I.IsConst = quals & Qualifiers::Const;
    I.IsVolatile = quals & Qualifiers::Volatile;
    I.IsVariadic = FPT->isVariadic();
    getASTVisitor().populate(I.ExceptionSpec, FPT);
    Inner = &I.ReturnType;
}

void
TypeInfoBuilder::
buildDecltype(
    DecltypeType const* T,
    unsigned quals,
    bool pack)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<DecltypeTypeInfo>);
    (*Inner)->Constraints = this->Constraints;
    (*Inner)->IsPackExpansion = pack;

    auto &I = dynamic_cast<DecltypeTypeInfo &>(**Inner);
    getASTVisitor().populate(I.Operand, T->getUnderlyingExpr());
    I.IsConst = quals & Qualifiers::Const;
    I.IsVolatile = quals & Qualifiers::Volatile;

    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildAuto(
    AutoType const* T,
    unsigned const quals,
    bool const pack)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<AutoTypeInfo>);
    (*Inner)->Constraints = this->Constraints;
    (*Inner)->IsPackExpansion = pack;

    auto &I = dynamic_cast<AutoTypeInfo &>(**Inner);
    I.IsConst = quals & Qualifiers::Const;
    I.IsVolatile = quals & Qualifiers::Volatile;
    I.Keyword = toAutoKind(T->getKeyword());
    if(T->isConstrained())
    {
        Optional<ArrayRef<TemplateArgument>> TArgs;
        if(auto Args = T->getTypeConstraintArguments();
            ! Args.empty())
        {
            TArgs.emplace(Args);
        }
        I.Constraint =
            getASTVisitor().toNameInfo(T->getTypeConstraintConcept(), TArgs);
        // Constraint->Prefix = getASTVisitor().buildNameInfo(
        //     cast<Decl>(CD->getDeclContext()));
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    Type const* T,
    unsigned quals,
    bool pack)
{
    MRDOCS_SYMBOL_TRACE(T, getASTVisitor().context_);
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
    (*Inner)->IsPackExpansion = pack;
    (*Inner)->Constraints = this->Constraints;

    auto &TI = dynamic_cast<NamedTypeInfo &>(**Inner);
    TI.IsConst = quals & Qualifiers::Const;
    TI.IsVolatile = quals & Qualifiers::Volatile;
    TI.Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);
    TI.Name->Name = getASTVisitor().toString(T);
    if (isa<BuiltinType>(T))
    {
        auto const* FT = cast<BuiltinType>(T);
        TI.FundamentalType = toFundamentalTypeKind(FT->getKind());
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    NestedNameSpecifier NNS,
    IdentifierInfo const* II,
    Optional<ArrayRef<TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
    (*Inner)->IsPackExpansion = pack;
    (*Inner)->Constraints = this->Constraints;

    auto &I = dynamic_cast<NamedTypeInfo &>(**Inner);
    I.IsConst = quals & Qualifiers::Const;
    I.IsVolatile = quals & Qualifiers::Volatile;

    if (TArgs)
    {
        I.Name = Polymorphic<NameInfo>(std::in_place_type<SpecializationNameInfo>);
        auto &Name = dynamic_cast<SpecializationNameInfo &>(*I.Name);
        if (II)
        {
            Name.Name = II->getName();
        }
        Name.Prefix = getASTVisitor().toNameInfo(NNS);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
    }
    else
    {
        I.Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);
        auto &Name = *I.Name;
        if (II)
        {
            Name.Name = II->getName();
        }
        Name.Prefix = getASTVisitor().toNameInfo(NNS);
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeInfoBuilder::
buildTerminal(
    NestedNameSpecifier NNS,
    NamedDecl* D,
    Optional<ArrayRef<TemplateArgument>> TArgs,
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

    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
    (*Inner)->IsPackExpansion = pack;
    (*Inner)->Constraints = this->Constraints;

    auto &TI = dynamic_cast<NamedTypeInfo &>(**Inner);
    TI.IsConst = quals & Qualifiers::Const;
    TI.IsVolatile = quals & Qualifiers::Volatile;

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
        TI.Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);
        populateNameInfo(*TI.Name, D);
    }
    else
    {
        TI.Name =
            Polymorphic<NameInfo>(std::in_place_type<SpecializationNameInfo>);
        auto &Name = dynamic_cast<SpecializationNameInfo &>(*TI.Name);
        populateNameInfo(Name, D);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

} // clang::mrdocs
