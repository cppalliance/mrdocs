//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_NOUNIQUEADDRESSATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_NOUNIQUEADDRESSATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The `[[no_unique_address]]` attribute (C++20): the member need not have a unique address.
*/
struct NoUniqueAddressAttribute final
    : AttributeCommonBase<AttributeKind::NoUniqueAddress>
{
};

MRDOCS_DESCRIBE_STRUCT(
    NoUniqueAddressAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_NOUNIQUEADDRESSATTRIBUTE_HPP
