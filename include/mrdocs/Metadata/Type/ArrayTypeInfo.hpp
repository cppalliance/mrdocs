//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_ARRAYTYPEINFO_HPP
#define MRDOCS_API_METADATA_TYPE_ARRAYTYPEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/AutoTypeInfo.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace clang::mrdocs {

struct ArrayTypeInfo final
    : TypeInfoCommonBase<TypeKind::Array>
{
    Polymorphic<TypeInfo> ElementType = Polymorphic<TypeInfo>(AutoTypeInfo{});
    ConstantExprInfo<std::uint64_t> Bounds;

    std::strong_ordering
    operator<=>(ArrayTypeInfo const&) const;
};

} // clang::mrdocs

#endif
