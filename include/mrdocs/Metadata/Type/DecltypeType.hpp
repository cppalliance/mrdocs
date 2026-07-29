//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_DECLTYPETYPE_HPP
#define MRDOCS_API_METADATA_TYPE_DECLTYPETYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** `decltype(expr)` type wrapper.
*/
struct DecltypeType final
    : TypeCommonBase<TypeKind::Decltype>
{
    /** Operand expression for decltype.
    */
    ExprInfo Operand;

};

MRDOCS_DESCRIBE_STRUCT(
    DecltypeType,
    (Type),
    (Operand)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_DECLTYPETYPE_HPP
