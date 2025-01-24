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

#ifndef MRDOCS_LIB_AST_NAMEINFOBUILDER_HPP
#define MRDOCS_LIB_AST_NAMEINFOBUILDER_HPP

#include <mrdocs/ADT/PolymorphicValue.hpp>
#include <lib/AST/TerminalTypeVisitor.hpp>
#include <clang/AST/TypeVisitor.h>

namespace clang::mrdocs {

class NameInfoBuilder
    : public TerminalTypeVisitor<NameInfoBuilder>
{
    PolymorphicValue<NameInfo> Result;

public:
    using TerminalTypeVisitor::TerminalTypeVisitor;

    PolymorphicValue<NameInfo>
    result()
    {
        return std::move(Result);
    }

    void
    buildDecltype(
        const DecltypeType* T,
        unsigned quals,
        bool pack);

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
        std::optional<ArrayRef<TemplateArgument>> const& TArgs,
        unsigned quals,
        bool pack);
};

} // clang::mrdocs

#endif
