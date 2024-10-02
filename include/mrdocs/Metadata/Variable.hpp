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

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <memory>

namespace clang {
namespace mrdocs {

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VariableInfo
    : InfoCommonBase<InfoKind::Variable>
    , SourceInfo
{
    /** The type of the variable */
    std::unique_ptr<TypeInfo> Type;

    std::unique_ptr<TemplateInfo> Template;

    ExprInfo Initializer;

    StorageClassKind StorageClass = StorageClassKind::None;

    ConstexprKind Constexpr = ConstexprKind::None;

    bool IsConstinit = false;

    bool IsThreadLocal = false;

    //--------------------------------------------

    explicit VariableInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
