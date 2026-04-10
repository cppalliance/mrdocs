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

#ifndef MRDOCS_API_METADATA_TARG_CONSTANTTARG_HPP
#define MRDOCS_API_METADATA_TARG_CONSTANTTARG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Support/CompareReflectedType.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Non-type template argument.
*/
struct ConstantTArg final
    : TArgCommonBase<TArgKind::Constant>
{
    /** Template argument expression.
    */
    ExprInfo Value;

};

MRDOCS_DESCRIBE_STRUCT(ConstantTArg, (TArgCommonBase<TArgKind::Constant>), (Value))

} // mrdocs

#endif // MRDOCS_API_METADATA_TARG_CONSTANTTARG_HPP
