//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_ARRAYTYPE_HPP
#define MRDOCS_API_METADATA_TYPE_ARRAYTYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/AutoType.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace mrdocs {

struct ArrayType final
    : TypeCommonBase<TypeKind::Array>
{
    Polymorphic<Type> ElementType = Polymorphic<Type>(AutoType{});
    ConstantExprInfo<std::uint64_t> Bounds;

    std::strong_ordering
    operator<=>(ArrayType const&) const;
};

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_ARRAYTYPE_HPP
