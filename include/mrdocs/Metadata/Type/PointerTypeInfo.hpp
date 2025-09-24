//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_POINTERTYPEINFO_HPP
#define MRDOCS_API_METADATA_TYPE_POINTERTYPEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace clang::mrdocs {

struct PointerTypeInfo final
    : TypeInfoCommonBase<TypeKind::Pointer>
{
    Optional<Polymorphic<TypeInfo>> PointeeType = std::nullopt;
    std::strong_ordering
    operator<=>(PointerTypeInfo const&) const;
};

} // clang::mrdocs

#endif
