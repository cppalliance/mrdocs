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
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Represents `auto` or `decltype(auto)` placeholder type.
*/
struct AutoType final
    : TypeCommonBase<TypeKind::Auto>
{
    /** Which placeholder keyword appears (`auto` or `decltype(auto)`).
    */
    AutoKind Keyword = AutoKind::Auto;

    /** Constraint on the auto type, if any.
    */
    Optional<Polymorphic<Name>> Constraint = std::nullopt;

};

MRDOCS_DESCRIBE_STRUCT(
    AutoType,
    (Type),
    (Keyword, Constraint)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_AUTOTYPE_HPP
