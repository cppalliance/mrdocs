//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_TPARAMKINDS_HPP
#define MRDOCS_LIB_METADATA_TPARAMKINDS_HPP

#include <mrdocs/Metadata/TParam/ConstantTParam.hpp>
#include <mrdocs/Metadata/TParam/TParamBase.hpp>
#include <mrdocs/Metadata/TParam/TemplateTParam.hpp>
#include <mrdocs/Metadata/TParam/TypeTParam.hpp>
#include <mrdocs/Support/DescribeKinds.hpp>

namespace mrdocs {

#define INFO(Name) MRDOCS_KIND_ENTRY(TParam, Name##TParam)
MRDOCS_DESCRIBE_KINDS_BEGIN(TParam)
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(TParam)
#undef INFO

} // namespace mrdocs

#endif
