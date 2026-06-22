//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_MAYBEUNUSEDATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_MAYBEUNUSEDATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The `[[maybe_unused]]` attribute (C++17): suppresses unused-entity warnings.
*/
struct MaybeUnusedAttribute final
    : AttributeCommonBase<AttributeKind::MaybeUnused>
{
};

MRDOCS_DESCRIBE_STRUCT(
    MaybeUnusedAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_MAYBEUNUSEDATTRIBUTE_HPP
