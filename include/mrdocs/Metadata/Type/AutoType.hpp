//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_AUTOTYPE_HPP
#define MRDOCS_API_METADATA_TYPE_AUTOTYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Type/AutoKind.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace mrdocs {

struct AutoType final
    : TypeCommonBase<TypeKind::Auto>
{
    AutoKind Keyword = AutoKind::Auto;

    /** Constraint on the auto type, if any.
     */
    Optional<Polymorphic<Name>> Constraint = std::nullopt;

    std::strong_ordering
    operator<=>(AutoType const&) const;
};

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_AUTOTYPE_HPP
