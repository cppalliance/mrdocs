//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_VARIABLE_HPP
#define MRDOCS_API_METADATA_VARIABLE_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VariableInfo final
    : InfoCommonBase<InfoKind::Variable>
    , SourceInfo
{
    /** The type of the variable */
    PolymorphicValue<TypeInfo> Type;

    std::optional<TemplateInfo> Template;

    ExprInfo Initializer;

    StorageClassKind StorageClass = StorageClassKind::None;

    bool IsInline = false;

    bool IsConstexpr = false;

    bool IsConstinit = false;

    bool IsThreadLocal = false;

    std::vector<std::string> Attributes;

    //--------------------------------------------

    explicit VariableInfo(SymbolID const &ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // clang::mrdocs

#endif
