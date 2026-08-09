//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_HPP
#define MRDOCS_API_METADATA_NAME_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Name/SpecializationName.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

// Register Name's concrete kinds for the generic visit
// (Support/Reflection/Describe.hpp).
#define INFO(X) MRDOCS_KIND_ENTRY(Name, X##Name)
MRDOCS_DESCRIBE_KINDS_BEGIN(Name)
#include <mrdocs/Metadata/Name/NameNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Name)
#undef INFO

} // mrdocs

#endif
