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

#include <lib/AST/TerminalTypeVisitor.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <clang/AST/TypeVisitor.h>

namespace clang::mrdocs {

class NameInfoBuilder
    : public TerminalTypeVisitor<NameInfoBuilder>
{
    Optional<Polymorphic<NameInfo>> Result;

public:
    using TerminalTypeVisitor::TerminalTypeVisitor;

    Polymorphic<NameInfo>
    result()
    {
        MRDOCS_ASSERT(Result.has_value());
        return std::move(*Result);
    }

    constexpr bool
    hasResult() const noexcept
    {
        return Result.has_value();
    }

    void
    buildDecltype(
        DecltypeType const* T,
        unsigned quals,
        bool pack);

    void
    buildTerminal(
        Type const* T,
        unsigned quals,
        bool pack);

    void
    buildTerminal(
        NestedNameSpecifier NNS,
        IdentifierInfo const* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);

    void
    buildTerminal(
        NestedNameSpecifier NNS,
        NamedDecl const* D,
        std::optional<ArrayRef<TemplateArgument>> const& TArgs,
        unsigned quals,
        bool pack);
};

} // clang::mrdocs

#endif
