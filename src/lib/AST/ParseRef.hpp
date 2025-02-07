//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_PARSELOOKUPNAME_HPP
#define MRDOCS_LIB_PARSELOOKUPNAME_HPP

#include <optional>
#include <vector>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <llvm/ADT/SmallVector.h>
#include <string_view>

namespace clang::mrdocs {

struct ParsedRefComponent {
    std::string_view Name;
    OperatorKind Operator;
    llvm::SmallVector<std::string_view, 20> TemplateArguments;

    constexpr
    bool
    isOperator() const
    {
        return Operator != OperatorKind::None;
    }

    bool
    isSpecialization() const
    {
        return !TemplateArguments.empty();
    }
};

struct ParsedRef {
    bool IsFullyQualified = false;
    llvm::SmallVector<ParsedRefComponent, 20> Components;
    llvm::SmallVector<std::string_view, 20> FunctionParameters;
    ReferenceKind Kind = ReferenceKind::None;
    bool IsConst = false;
};

Expected<ParsedRef>
parseRef(std::string_view sv);

} // clang::mrdocs

#endif
