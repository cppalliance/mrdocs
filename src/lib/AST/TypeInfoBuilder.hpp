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

#ifndef MRDOCS_LIB_AST_TYPEINFOBUILDER_HPP
#define MRDOCS_LIB_AST_TYPEINFOBUILDER_HPP

#include <lib/AST/TerminalTypeVisitor.hpp>

namespace clang::mrdocs {

/** A builder class for type information.

    This class is used to build type information by visiting
    various terminal types.

    It derives from the TerminalTypeVisitor class and provides
    specific implementations for building different types of
    type information.
 */
class TypeInfoBuilder
    : public TerminalTypeVisitor<TypeInfoBuilder>
{
    std::unique_ptr<TypeInfo> Result;
    std::unique_ptr<TypeInfo>* Inner = &Result;

public:
    using TerminalTypeVisitor::TerminalTypeVisitor;

    std::unique_ptr<TypeInfo>
    result()
    {
        return std::move(Result);
    }

    void
    buildPointer(const PointerType* T, unsigned quals);

    void
    buildLValueReference(const LValueReferenceType* T);

    void
    buildRValueReference(const RValueReferenceType* T);

    void
    buildMemberPointer(const MemberPointerType* T, unsigned quals);

    void
    buildArray(const ArrayType* T);

    void
    populate(const FunctionType* T);

    void
    buildDecltype(
        const DecltypeType* T,
        unsigned quals,
        bool pack);

    void
    buildAuto(
        const AutoType* T,
        unsigned const quals,
        bool const pack);

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const Type* T,
        unsigned quals,
        bool pack);

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const IdentifierInfo* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const NamedDecl* D,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);
};

} // clang::mrdocs

#endif
