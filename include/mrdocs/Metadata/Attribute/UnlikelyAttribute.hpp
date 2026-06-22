//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_UNLIKELYATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_UNLIKELYATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The `[[unlikely]]` attribute (C++20): this path is less likely.
*/
struct UnlikelyAttribute final
    : AttributeCommonBase<AttributeKind::Unlikely>
{
};

MRDOCS_DESCRIBE_STRUCT(
    UnlikelyAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_UNLIKELYATTRIBUTE_HPP
