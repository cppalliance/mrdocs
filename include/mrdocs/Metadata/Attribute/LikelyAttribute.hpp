//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_LIKELYATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_LIKELYATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The `[[likely]]` attribute (C++20): this path is more likely.
*/
struct LikelyAttribute final
    : AttributeCommonBase<AttributeKind::Likely>
{
};

MRDOCS_DESCRIBE_STRUCT(
    LikelyAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_LIKELYATTRIBUTE_HPP
