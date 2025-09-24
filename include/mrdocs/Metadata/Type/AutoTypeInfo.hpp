//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_AUTOTYPEINFO_HPP
#define MRDOCS_API_METADATA_TYPE_AUTOTYPEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Type/AutoKind.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace clang::mrdocs {

struct AutoTypeInfo final
    : TypeInfoCommonBase<TypeKind::Auto>
{
    AutoKind Keyword = AutoKind::Auto;
    Optional<Polymorphic<NameInfo>> Constraint = std::nullopt;

    std::strong_ordering
    operator<=>(AutoTypeInfo const&) const;
};

} // clang::mrdocs

#endif
