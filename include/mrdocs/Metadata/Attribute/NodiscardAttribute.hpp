//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_NODISCARDATTRIBUTE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_NODISCARDATTRIBUTE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs {

/** The `[[nodiscard]]` attribute (C++17, reason added in C++20).

    Encourages the compiler to warn if the return value is discarded.
*/
struct NodiscardAttribute final
    : AttributeCommonBase<AttributeKind::Nodiscard>
{
    /** The reason, if any.

        This is the evaluated string argument, without quotes, given
        by the C++20 form `[[nodiscard("reason")]]`. Empty otherwise.
    */
    std::string Reason;
};

MRDOCS_DESCRIBE_STRUCT(
    NodiscardAttribute,
    (Attribute),
    (Reason)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_NODISCARDATTRIBUTE_HPP
