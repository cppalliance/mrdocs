//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_ASSUMEATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_ASSUMEATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs {

/** The `[[assume(expression)]]` attribute (C++23).

    States that the expression always evaluates to true at this point.
*/
struct AssumeAttribute final
    : AttributeCommonBase<AttributeKind::Assume>
{
    /** The assumed expression, as written.
    */
    std::string Expression;
};

MRDOCS_DESCRIBE_STRUCT(
    AssumeAttribute,
    (Attribute),
    (Expression)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_ASSUMEATTRIBUTE_HPP
