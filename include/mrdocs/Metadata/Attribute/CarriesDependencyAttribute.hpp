//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_CARRIESDEPENDENCYATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_CARRIESDEPENDENCYATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The `[[carries_dependency]]` attribute (C++11): release-consume dependency propagation.
*/
struct CarriesDependencyAttribute final
    : AttributeCommonBase<AttributeKind::CarriesDependency>
{
};

MRDOCS_DESCRIBE_STRUCT(
    CarriesDependencyAttribute,
    (Attribute),
    ()
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_CARRIESDEPENDENCYATTRIBUTE_HPP
