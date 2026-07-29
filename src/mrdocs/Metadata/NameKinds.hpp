//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_NAMEKINDS_HPP
#define MRDOCS_LIB_METADATA_NAMEKINDS_HPP

#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Name/SpecializationName.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

#define INFO(Name_) MRDOCS_KIND_ENTRY(Name, Name_##Name)
MRDOCS_DESCRIBE_KINDS_BEGIN(Name)
#include <mrdocs/Metadata/Name/NameNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Name)
#undef INFO

} // namespace mrdocs

#endif
