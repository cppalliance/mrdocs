//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_NAMEDTYPEINFO_HPP
#define MRDOCS_API_METADATA_TYPE_NAMEDTYPEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name/IdentifierNameInfo.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Type/FundamentalTypeKind.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Metadata/Type/TypeKind.hpp>
#include <optional>

namespace clang::mrdocs {

struct NamedTypeInfo final
    : TypeInfoCommonBase<TypeKind::Named>
{
    Polymorphic<NameInfo> Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);

    std::optional<FundamentalTypeKind> FundamentalType;

    std::strong_ordering
    operator<=>(NamedTypeInfo const& other) const;
};

} // clang::mrdocs

#endif
