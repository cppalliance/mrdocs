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

#ifndef MRDOCS_API_METADATA_TYPE_NAMEDTYPE_HPP
#define MRDOCS_API_METADATA_TYPE_NAMEDTYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Type/FundamentalTypeKind.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Metadata/Type/TypeKind.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <optional>

namespace mrdocs {

/** A type identified by name (possibly fundamental).
*/
struct NamedType final
    : TypeCommonBase<TypeKind::Named>
{
    /** The name of the type (identifier or specialization).
    */
    Polymorphic<struct Name> Name = Polymorphic<struct Name>(std::in_place_type<IdentifierName>);

    /** Fundamental kind if this named type is a built-in.
    */
    Optional<FundamentalTypeKind> FundamentalType;

};

MRDOCS_DESCRIBE_STRUCT(
    NamedType,
    (Type),
    (Name, FundamentalType)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_NAMEDTYPE_HPP
