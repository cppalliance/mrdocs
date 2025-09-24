//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_DECLTYPETYPEINFO_HPP
#define MRDOCS_API_METADATA_TYPE_DECLTYPETYPEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace clang::mrdocs {

struct DecltypeTypeInfo final
    : TypeInfoCommonBase<TypeKind::Decltype>
{
    ExprInfo Operand;

    auto operator<=>(DecltypeTypeInfo const&) const = default;
};

} // clang::mrdocs

#endif
