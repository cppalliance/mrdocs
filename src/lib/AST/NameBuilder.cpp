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

#include <lib/AST/NameBuilder.hpp>

namespace mrdocs {

void
NameBuilder::
buildDecltype(
    clang::DecltypeType const*,
    unsigned,
    bool)
{
    // KRYSTIAN TODO: support decltype in names
    // (e.g. within nested-name-specifiers).
}

void
NameBuilder::
buildTerminal(
    clang::Type const* T,
    unsigned,
    bool)
{
    Result = Polymorphic<Name>(std::in_place_type<IdentifierName>);
    (*Result)->Identifier = getASTVisitor().toString(T);
}

void
NameBuilder::
buildTerminal(
    clang::NestedNameSpecifier NNS,
    clang::IdentifierInfo const* II,
    Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
    unsigned,
    bool)
{
    if (TArgs)
    {
        Result =
            Polymorphic<Name>(std::in_place_type<SpecializationName>);
        if (II)
        {
            (*Result)->Identifier = II->getName();
        }
        getASTVisitor().populate(
            static_cast<SpecializationName&>(**Result).TemplateArgs,
            *TArgs);
    }
    else
    {
        Result = Polymorphic<Name>(std::in_place_type<IdentifierName>);
        if (II)
        {
            (*Result)->Identifier = II->getName();
        }
    }
    if (NNS)
    {
        (*Result)->Prefix = getASTVisitor().toName(NNS);
    }
}

void
NameBuilder::
buildTerminal(
    clang::NestedNameSpecifier NNS,
    clang::NamedDecl const* D,
    Optional<llvm::ArrayRef<clang::TemplateArgument>> const& TArgs,
    unsigned,
    bool)
{
    // Look for the Info type. If this is a template specialization,
    // we look for the Info of the specialized record.
    clang::Decl const* ID = decayToPrimaryTemplate(D);


    auto populateName = [&](Name& Name, clang::NamedDecl const* DU)
    {
        if(clang::IdentifierInfo const* II = DU->getIdentifier())
        {
            Name.Identifier = II->getName();
        }
        if (Symbol const* I = getASTVisitor().findOrTraverse(const_cast<clang::Decl*>(ID)))
        {
            Name.id = I->id;
        }
        if (NNS)
        {
            Name.Prefix = getASTVisitor().toName(NNS);
        }
    };

    if (!TArgs)
    {
        Result = Polymorphic<Name>(std::in_place_type<IdentifierName>);
        populateName(**Result, D);
    }
    else
    {
        Result = Polymorphic<Name>(
            std::in_place_type<SpecializationName>);
        populateName(**Result, D);
        MRDOCS_ASSERT(Result);
        getASTVisitor().populate(
            (*Result)->asSpecialization().TemplateArgs,
            *TArgs);
    }
}

} // mrdocs
