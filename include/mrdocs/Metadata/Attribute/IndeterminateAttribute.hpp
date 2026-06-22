//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_INDETERMINATEATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_INDETERMINATEATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The `[[indeterminate]]` attribute (C++26): the object has an indeterminate value if uninitialized.
*/
struct IndeterminateAttribute final
    : AttributeCommonBase<AttributeKind::Indeterminate>
{
};

MRDOCS_DESCRIBE_STRUCT(
    IndeterminateAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_INDETERMINATEATTRIBUTE_HPP
