//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTES_HPP
#define MRDOCS_API_METADATA_ATTRIBUTES_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Attribute/AssumeAttribute.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Metadata/Attribute/AttributeKind.hpp>
#include <mrdocs/Metadata/Attribute/CarriesDependencyAttribute.hpp>
#include <mrdocs/Metadata/Attribute/DeprecatedAttribute.hpp>
#include <mrdocs/Metadata/Attribute/FallthroughAttribute.hpp>
#include <mrdocs/Metadata/Attribute/IndeterminateAttribute.hpp>
#include <mrdocs/Metadata/Attribute/LikelyAttribute.hpp>
#include <mrdocs/Metadata/Attribute/MaybeUnusedAttribute.hpp>
#include <mrdocs/Metadata/Attribute/NoUniqueAddressAttribute.hpp>
#include <mrdocs/Metadata/Attribute/NodiscardAttribute.hpp>
#include <mrdocs/Metadata/Attribute/NoreturnAttribute.hpp>
#include <mrdocs/Metadata/Attribute/OtherAttribute.hpp>
#include <mrdocs/Metadata/Attribute/UnlikelyAttribute.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

// Register Attribute's concrete kinds so the generic `visit`
// (Support/Reflection/Describe.hpp) can dispatch over them.
#define INFO(X) MRDOCS_KIND_ENTRY(Attribute, X##Attribute)
MRDOCS_DESCRIBE_KINDS_BEGIN(Attribute)
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Attribute)
#undef INFO

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTES_HPP
