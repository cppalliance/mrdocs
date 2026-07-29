//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORDKEYKIND_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORDKEYKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs {

/** The kind of record: struct, class, or union.
*/
enum class RecordKeyKind
{
    /// A struct.
    Struct,
    /// A C++ class.
    Class,
    /// A C-style Union
    Union
};

MRDOCS_DESCRIBE_ENUM(
    RecordKeyKind,
    Struct, Class, Union)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORDKEYKIND_HPP
