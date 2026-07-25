//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TARG_TARGKIND_HPP
#define MRDOCS_API_METADATA_TARG_TARGKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string_view>

namespace mrdocs {

/** The kind of template argument.
*/
enum class TArgKind : int {
#define INFO(Type) Type,
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
};

MRDOCS_DESCRIBE_ENUM_BEGIN(TArgKind)
#define INFO(Name) MRDOCS_ENUM_ENTRY(TArgKind, Name)
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
MRDOCS_DESCRIBE_ENUM_END(TArgKind)

} // mrdocs

#endif // MRDOCS_API_METADATA_TARG_TARGKIND_HPP
