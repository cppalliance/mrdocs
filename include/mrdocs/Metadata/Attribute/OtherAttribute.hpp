//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_OTHERATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_OTHERATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** An attribute mrdocs does not treat specially.

    The spelling is preserved in @ref Attribute::Name and the
    arguments in @ref Attribute::balancedTokens.
*/
struct OtherAttribute final
    : AttributeCommonBase<AttributeKind::Other>
{
};

MRDOCS_DESCRIBE_STRUCT(
    OtherAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_OTHERATTRIBUTE_HPP
