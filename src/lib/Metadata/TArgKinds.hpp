//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_TARGKINDS_HPP
#define MRDOCS_LIB_METADATA_TARGKINDS_HPP

#include <mrdocs/Metadata/TArg/ConstantTArg.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/TArg/TemplateTArg.hpp>
#include <mrdocs/Metadata/TArg/TypeTArg.hpp>
#include <mrdocs/Support/DescribeKinds.hpp>

namespace mrdocs {

#define INFO(Name) MRDOCS_KIND_ENTRY(TArg, Name##TArg)
MRDOCS_DESCRIBE_KINDS_BEGIN(TArg)
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(TArg)
#undef INFO

} // namespace mrdocs

#endif
