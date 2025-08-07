//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_PARSEREF_HPP
#define MRDOCS_LIB_PARSEREF_HPP

#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Parse.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <llvm/ADT/SmallVector.h>
#include <string_view>

namespace clang::mrdocs {

struct TArg;

struct ParsedRefComponent {
    // Component name
    std::string_view Name;

    // If not empty, this is a specialization
    bool HasTemplateArguments = false;
    std::vector<Polymorphic<TArg>> TemplateArguments;

    // If not None, this is an operator
    // Only the last component can be an operator
    OperatorKind Operator;

    // If not empty, this is a conversion operator
    // Only the last component can be a conversion operator
    Polymorphic<TypeInfo> ConversionType = std::nullopt;

    constexpr
    bool
    isOperator() const
    {
        return Operator != OperatorKind::None;
    }

    constexpr
    bool
    isConversion() const
    {
        return static_cast<bool>(ConversionType);
    }

    bool
    isSpecialization() const
    {
        return !TemplateArguments.empty();
    }
};

struct ParsedRef {
    bool IsFullyQualified = false;
    llvm::SmallVector<ParsedRefComponent, 8> Components;

    // The following are populated when the last element is a function
    bool HasFunctionParameters = false;
    llvm::SmallVector<Polymorphic<TypeInfo>, 8> FunctionParameters;
    bool IsVariadic = false;
    bool IsExplicitObjectMemberFunction = false;
    ReferenceKind Kind = ReferenceKind::None;
    bool IsConst = false;
    bool IsVolatile = false;
    NoexceptInfo ExceptionSpec;
};

ParseResult
parse(
    char const* first,
    char const* last,
    ParsedRef& value);

} // clang::mrdocs

#endif
