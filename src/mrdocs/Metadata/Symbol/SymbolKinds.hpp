//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_SYMBOL_SYMBOLKINDS_HPP
#define MRDOCS_LIB_METADATA_SYMBOL_SYMBOLKINDS_HPP

#include <mrdocs/Metadata/Symbol/Concept.hpp>
#include <mrdocs/Metadata/Symbol/Enum.hpp>
#include <mrdocs/Metadata/Symbol/EnumConstant.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/Guide.hpp>
#include <mrdocs/Metadata/Symbol/Macro.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/NamespaceAlias.hpp>
#include <mrdocs/Metadata/Symbol/Overloads.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Metadata/Symbol/Typedef.hpp>
#include <mrdocs/Metadata/Symbol/Using.hpp>
#include <mrdocs/Metadata/Symbol/Variable.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

#define INFO(Name) MRDOCS_KIND_ENTRY(Symbol, Name##Symbol)
MRDOCS_DESCRIBE_KINDS_BEGIN(Symbol)
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Symbol)
#undef INFO

} // namespace mrdocs

#endif
