//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_RVALUEREFERENCETYPE_HPP
#define MRDOCS_API_METADATA_TYPE_RVALUEREFERENCETYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace mrdocs {

struct RValueReferenceType final
    : TypeCommonBase<TypeKind::RValueReference>
{
    Polymorphic<Type> PointeeType = Polymorphic<Type>(AutoType{});

    std::strong_ordering
    operator<=>(RValueReferenceType const&) const;
};

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_RVALUEREFERENCETYPE_HPP
