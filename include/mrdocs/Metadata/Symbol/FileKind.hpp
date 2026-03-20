//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_FILEKIND_HPP
#define MRDOCS_API_METADATA_SYMBOL_FILEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Nullable.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs {

/** Classifies where a file originates from.
*/
enum class FileKind
{
    /// File in the source directory
    Source,
    /// File in a system include directory
    System,
    /// File outside the source directory
    Other
};

MRDOCS_DESCRIBE_ENUM(
    FileKind,
    Source, System, Other)

/** Map a FileKind into a DOM value.
    @param v Destination value to populate.
    @param kind File category to serialize.
*/
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FileKind kind);

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_FILEKIND_HPP
