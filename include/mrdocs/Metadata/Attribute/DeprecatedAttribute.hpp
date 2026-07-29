//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_DEPRECATEDATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_DEPRECATEDATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs {

/** The `[[deprecated]]` attribute (C++14).

    Indicates that the use of the entity is allowed but discouraged.
*/
struct DeprecatedAttribute final
    : AttributeCommonBase<AttributeKind::Deprecated>
{
    /** The deprecation message, if any.

        This is the evaluated string argument, without quotes, e.g.
        `use bar instead` for `[[deprecated("use bar instead")]]`.
        Empty when no message was given.
    */
    std::string Message;
};

MRDOCS_DESCRIBE_STRUCT(
    DeprecatedAttribute,
    (Attribute),
    (Message)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_DEPRECATEDATTRIBUTE_HPP
