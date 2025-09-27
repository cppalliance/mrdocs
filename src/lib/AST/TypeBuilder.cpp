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

#include <lib/AST/TypeBuilder.hpp>

namespace mrdocs {

void
TypeBuilder::
buildPointer(clang::PointerType const*, unsigned quals)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<PointerType>);
    auto &I = (*Inner)->asPointer();
    I.IsConst = quals & clang::Qualifiers::Const;
    I.IsVolatile = quals & clang::Qualifiers::Volatile;
    Inner = &I.PointeeType;
}

void
TypeBuilder::
buildLValueReference(clang::LValueReferenceType const*)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<LValueReferenceType>);
    Inner = &(*Inner)->asLValueReference().PointeeType;
}

void
TypeBuilder::
buildRValueReference(clang::RValueReferenceType const*)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<RValueReferenceType>);
    Inner = &(*Inner)->asRValueReference().PointeeType;
}

void
TypeBuilder::
buildMemberPointer(
    clang::MemberPointerType const* T,
    unsigned const quals)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<MemberPointerType>);
    auto &I = (*Inner)->asMemberPointer();
    I.IsConst = quals & clang::Qualifiers::Const;
    I.IsVolatile = quals & clang::Qualifiers::Volatile;
    // do not set NNS because the parent type is *not*
    // a nested-name-specifier which qualifies the pointee type
    I.ParentType =
        getASTVisitor().toType(clang::QualType(T->getQualifier().getAsType(), 0));
    Inner = &I.PointeeType;
}

void
TypeBuilder::
buildArray(clang::ArrayType const* T)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<ArrayType>);
    auto &I = (*Inner)->asArray();
    if (auto* CAT = dyn_cast<clang::ConstantArrayType>(T))
    {
        getASTVisitor().populate(I.Bounds, CAT->getSizeExpr(), CAT->getSize());
    }
    else if (auto* DAT = dyn_cast<clang::DependentSizedArrayType>(T))
    {
        getASTVisitor().populate(I.Bounds, DAT->getSizeExpr());
    }
    Inner = &I.ElementType;
}

void
TypeBuilder::
populate(clang::FunctionType const* T)
{
    auto* FPT = cast<clang::FunctionProtoType>(T);
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<FunctionType>);
    auto &I = (*Inner)->asFunction();
    for (clang::QualType const PT : FPT->getParamTypes())
    {
        I.ParamTypes.emplace_back(getASTVisitor().toType(PT));
    }
    I.RefQualifier = toReferenceKind(
        FPT->getRefQualifier());
    unsigned const quals = FPT->getMethodQuals().getFastQualifiers();
    I.IsConst = quals & clang::Qualifiers::Const;
    I.IsVolatile = quals & clang::Qualifiers::Volatile;
    I.IsVariadic = FPT->isVariadic();
    getASTVisitor().populate(I.ExceptionSpec, FPT);
    Inner = &I.ReturnType;
}

void
TypeBuilder::
buildDecltype(
    clang::DecltypeType const* T,
    unsigned quals,
    bool pack)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<DecltypeType>);
    (*Inner)->Constraints = this->Constraints;
    (*Inner)->IsPackExpansion = pack;

    auto &I = (*Inner)->asDecltype();
    getASTVisitor().populate(I.Operand, T->getUnderlyingExpr());
    I.IsConst = quals & clang::Qualifiers::Const;
    I.IsVolatile = quals & clang::Qualifiers::Volatile;

    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeBuilder::
buildAuto(
    clang::AutoType const* T,
    unsigned const quals,
    bool const pack)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<AutoType>);
    (*Inner)->Constraints = this->Constraints;
    (*Inner)->IsPackExpansion = pack;

    auto &I = (*Inner)->asAuto();
    I.IsConst = quals & clang::Qualifiers::Const;
    I.IsVolatile = quals & clang::Qualifiers::Volatile;
    I.Keyword = toAutoKind(T->getKeyword());
    if(T->isConstrained())
    {
        Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs;
        if(auto Args = T->getTypeConstraintArguments();
            ! Args.empty())
        {
            TArgs.emplace(Args);
        }
        I.Constraint =
            getASTVisitor().toName(T->getTypeConstraintConcept(), TArgs);
        // Constraint->Prefix = getASTVisitor().buildName(
        //     cast<Decl>(CD->getDeclContext()));
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeBuilder::
buildTerminal(
    clang::Type const* T,
    unsigned quals,
    bool pack)
{
    MRDOCS_SYMBOL_TRACE(T, getASTVisitor().context_);
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<NamedType>);
    (*Inner)->IsPackExpansion = pack;
    (*Inner)->Constraints = this->Constraints;

    auto &TI = (*Inner)->asNamed();
    TI.IsConst = quals & clang::Qualifiers::Const;
    TI.IsVolatile = quals & clang::Qualifiers::Volatile;
    TI.Name = Polymorphic<Name>(std::in_place_type<IdentifierName>);
    TI.Name->Identifier = getASTVisitor().toString(T);
    if (isa<clang::BuiltinType>(T))
    {
        auto const* FT = cast<clang::BuiltinType>(T);
        TI.FundamentalType = toFundamentalTypeKind(FT->getKind());
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeBuilder::
buildTerminal(
    clang::NestedNameSpecifier NNS,
    clang::IdentifierInfo const* II,
    Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<NamedType>);
    (*Inner)->IsPackExpansion = pack;
    (*Inner)->Constraints = this->Constraints;

    auto &I = (*Inner)->asNamed();
    I.IsConst = quals & clang::Qualifiers::Const;
    I.IsVolatile = quals & clang::Qualifiers::Volatile;

    if (TArgs)
    {
        I.Name = Polymorphic<Name>(std::in_place_type<SpecializationName>);
        auto &Name = I.Name->asSpecialization();
        if (II)
        {
            Name.Identifier = II->getName();
        }
        Name.Prefix = getASTVisitor().toName(NNS);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
    }
    else
    {
        I.Name = Polymorphic<Name>(std::in_place_type<IdentifierName>);
        auto &Name = *I.Name;
        if (II)
        {
            Name.Identifier = II->getName();
        }
        Name.Prefix = getASTVisitor().toName(NNS);
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

void
TypeBuilder::
buildTerminal(
    clang::NestedNameSpecifier NNS,
    clang::NamedDecl* D,
    Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
    unsigned quals,
    bool pack)
{
    MRDOCS_SYMBOL_TRACE(NNS, getASTVisitor().context_);
    MRDOCS_SYMBOL_TRACE(D, getASTVisitor().context_);
    MRDOCS_SYMBOL_TRACE(TArgs, getASTVisitor().context_);

    // Look for the Info type. If this is a template specialization,
    // we look for the Info of the specialized record.
    clang::Decl const* ID = decayToPrimaryTemplate(D);
    MRDOCS_SYMBOL_TRACE(ID, getASTVisitor().context_);

    MRDOCS_ASSERT(Inner);
    *Inner = Polymorphic<Type>(std::in_place_type<NamedType>);
    (*Inner)->IsPackExpansion = pack;
    (*Inner)->Constraints = this->Constraints;

    auto &TI = (*Inner)->asNamed();
    TI.IsConst = quals & clang::Qualifiers::Const;
    TI.IsVolatile = quals & clang::Qualifiers::Volatile;

    auto populateName = [&](Name& Name, clang::NamedDecl* D)
    {
        if(clang::IdentifierInfo const* II = D->getIdentifier())
        {
            Name.Identifier = II->getName();
        }
        if (Symbol const* I = getASTVisitor().findOrTraverse(const_cast<clang::Decl*>(ID)))
        {
            Name.id = I->id;
        }
        if(NNS)
        {
            Name.Prefix = getASTVisitor().toName(NNS);
        }
    };

    if (!TArgs)
    {
        TI.Name = Polymorphic<Name>(std::in_place_type<IdentifierName>);
        populateName(*TI.Name, D);
    }
    else
    {
        TI.Name =
            Polymorphic<Name>(std::in_place_type<SpecializationName>);
        auto &Name = TI.Name->asSpecialization();
        populateName(Name, D);
        getASTVisitor().populate(Name.TemplateArgs, *TArgs);
    }
    Result->Constraints = this->Constraints;
    Result->IsPackExpansion = pack;
}

} // mrdocs
