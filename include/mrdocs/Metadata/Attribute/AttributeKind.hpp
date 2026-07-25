//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEKIND_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** The kind of a C++ attribute.

    This identifies the standard C++ attributes mrdocs treats
    specially, regardless of how they were spelled in the source.
    For example, `[[deprecated]]`, `[[gnu::deprecated]]`, and
    `__declspec(deprecated)` all share the deprecated kind.

    Attributes mrdocs does not recognize use the `Other` kind.

*/
enum class AttributeKind {
#define INFO(PascalName) PascalName,
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
};

MRDOCS_DESCRIBE_ENUM_BEGIN(AttributeKind)
#define INFO(Name) MRDOCS_ENUM_ENTRY(AttributeKind, Name)
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
MRDOCS_DESCRIBE_ENUM_END(AttributeKind)

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEKIND_HPP
