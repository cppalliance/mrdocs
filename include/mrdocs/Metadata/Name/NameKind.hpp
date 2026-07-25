//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_NAMEKIND_HPP
#define MRDOCS_API_METADATA_NAME_NAMEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Kinds of names that appear in type and symbol metadata.
*/
enum class NameKind {
#define INFO(Type) Type,
#include <mrdocs/Metadata/Name/NameNodes.inc>
};

MRDOCS_DESCRIBE_ENUM_BEGIN(NameKind)
#define INFO(Name) MRDOCS_ENUM_ENTRY(NameKind, Name)
#include <mrdocs/Metadata/Name/NameNodes.inc>
MRDOCS_DESCRIBE_ENUM_END(NameKind)

} // mrdocs

#endif
